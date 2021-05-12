/* pixelsort -- a GEGL operation that implements a pixel sorting effect
 * Copyright (C) 2021 Evergreen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define GETTEXT_PACKAGE "pixelsort"
#include <glib/gi18n-lib.h>
#include <stdio.h>

#ifdef GEGL_PROPERTIES

enum_start (gegl_pixelsort_mode)
  enum_value (GEGL_PIXELSORT_MODE_WHITE, "white", "White")
  enum_value (GEGL_PIXELSORT_MODE_BLACK, "black", "Black")
  enum_value (GEGL_PIXELSORT_MODE_LUM, "luminance", "Luminance")
enum_end (GeglPixelsortMode)

property_enum (mode, "Mode",
               GeglPixelsortMode, gegl_pixelsort_mode,
               GEGL_PIXELSORT_MODE_WHITE)
  description ("Property used to sort the pixels")

property_enum (direction, "Sort direction",
               GeglOrientation, gegl_orientation,
               GEGL_ORIENTATION_HORIZONTAL)

property_boolean (reverse, "Reverse", FALSE)
     description ("Reverse sort order")

property_double (threshold, "Threshold", 0.1)
    description ("Determines how much of each row/column is sorted")
    value_range (0.0, 1.0)

property_seed (seed, "Random seed", rand)

#else

#define GEGL_OP_AREA_FILTER
#define GEGL_OP_NAME     pixelsort
#define GEGL_OP_C_SOURCE pixelsort.c

#include "gegl-op.h"

static gdouble
get_key (gfloat *pixel, GeglPixelsortMode mode)
{
  gfloat max;
  switch (mode)
    {
    case GEGL_PIXELSORT_MODE_LUM:
      return 0.2126 * pixel[0] + 0.7152 * pixel[1] + 0.0722 * pixel[2];
    case GEGL_PIXELSORT_MODE_WHITE:
    case GEGL_PIXELSORT_MODE_BLACK:
      max = pixel[0];
      if (pixel[1] > max)
        max = pixel[1];
      if (pixel[2] > max)
        max = pixel[2];
      return max;
    default:
      return 0.0;
    }
}

static void
prepare (GeglOperation *operation)
{
  const Babl *space = gegl_operation_get_source_space (operation, "input");

  gegl_operation_set_format (operation, "input", babl_format_with_space ("RGBA float", space));
  gegl_operation_set_format (operation, "output", babl_format_with_space ("RGBA float", space));
}

static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result,
         gint                 level)
  {
  GeglProperties   *o = GEGL_PROPERTIES (operation);
  const Babl       *format = gegl_operation_get_format (operation, "output");
  GeglPixelsortMode mode = o->mode;
  gint              size, length, i, pos;
  GeglRectangle     dst_rect;
  gfloat           *src_buf;

  if (o->direction == GEGL_ORIENTATION_HORIZONTAL)
    {
      size = result->height;
      length = result->width;
      dst_rect.width  = length;
      dst_rect.height = 1;
      pos = result->y;
    }
  else
    {
      size = result->width;
      length = result->height;
      dst_rect.width  = 1;
      dst_rect.height = length;
      pos = result->x;
    }

  printf("length: %d\n", length);
  src_buf = g_new0 (gfloat, length * 4);

  dst_rect.x = result->x;
  dst_rect.y = result->y;

  for (i = 0; i < size; i++)
    {
      GeglRectangle src_rect;

      if (o->direction == GEGL_ORIENTATION_HORIZONTAL)
        {
          dst_rect.y = i + result->y;
          src_rect = dst_rect;
        }
      else
        {
          dst_rect.x = i + result->x;
          src_rect = dst_rect;
        }

      gegl_buffer_get (input, &src_rect, 1.0, format, src_buf,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);

      gint start = 0;
      gint end = length;
      gboolean in_thresh = FALSE;
      gint j;
      for (j = 0; j < length * 4; j += 4)
        {
          gdouble key = get_key (&src_buf[j], mode);
          if (key >= o->threshold && !in_thresh)
            {
              start = j;
              in_thresh = TRUE;
            }
          else if (key < o->threshold && in_thresh)
            {
              end = j;
              break;
            }
        }
      for (j = 0; j < length * 4; j += 4)
        {
          int k = j;
          while (k > 0)
            {
              gdouble key1 = get_key(&src_buf[k], mode);
              gdouble key2 = get_key(&src_buf[k - 4], mode);

              gboolean in_place = o->reverse ? (key2 > key1) : (key1 > key2);
              if (in_place)
                {
                  break;
                }
              gfloat r = src_buf[k];
              gfloat g = src_buf[k + 1];
              gfloat b = src_buf[k + 2];
              gfloat a = src_buf[k + 3];
              src_buf[k] = src_buf[k - 4];
              src_buf[k + 1] = src_buf[k - 3];
              src_buf[k + 2] = src_buf[k - 2];
              src_buf[k + 3] = src_buf[k - 1];
              src_buf[k - 4] = r;
              src_buf[k - 3] = g;
              src_buf[k - 2] = b;
              src_buf[k - 1] = a;
              k -= 4;
            }
        }
      gegl_buffer_set (output, &dst_rect, 0, format, src_buf,
                       GEGL_AUTO_ROWSTRIDE);
    }

  g_free(src_buf);

  return TRUE;
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationFilterClass *filter_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

  filter_class->process    = process;
  operation_class->prepare = prepare;

  gegl_operation_class_set_keys (operation_class,
    "name",        "gegl:pixelsort",
    "title",       "Pixel Sort",
    "categories",  "distort",
    "license",     "GPL3+",
    "description", "Sorts pixels by different properties within a threshold",
    NULL);
}

#endif
