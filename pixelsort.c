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
  enum_value (GEGL_PIXELSORT_MODE_LUMINANCE, "luminance", "Luminance")
  enum_value (GEGL_PIXELSORT_MODE_WHITE, "white", "White")
  enum_value (GEGL_PIXELSORT_MODE_BLACK, "black", "Black")
enum_end (GeglPixelsortMode)

property_enum (mode, "Mode",
               GeglPixelsortMode, gegl_pixelsort_mode,
               GEGL_PIXELSORT_MODE_LUMINANCE)
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

#define GEGL_OP_FILTER
#define GEGL_OP_NAME     pixelsort
#define GEGL_OP_C_SOURCE pixelsort.c

#include "gegl-op.h"

static gdouble
get_key (gfloat *pixel, GeglPixelsortMode mode)
{
  gfloat max;
  switch (mode)
    {
    case GEGL_PIXELSORT_MODE_LUMINANCE:
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

static GeglRectangle
get_cached_region (GeglOperation       *self,
                   const GeglRectangle *roi)
{
  const GeglRectangle *in_rect
      = gegl_operation_source_get_bounding_box (self, "input");

  if (in_rect && !gegl_rectangle_is_infinite_plane (in_rect))
    return *in_rect;

  return *roi;
}

static GeglRectangle
get_required_for_output (GeglOperation       *self,
                         const gchar         *input_pad,
                         const GeglRectangle *roi)
{
  return get_cached_region (self, roi);
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
  gint              size, length, i, j;
  GeglRectangle     line_rect;
  gfloat           *line_buf;

  if (o->direction == GEGL_ORIENTATION_HORIZONTAL)
    {
      size = result->height;
      length = result->width;
      line_rect.width  = length;
      line_rect.height = 1;
    }
  else
    {
      size = result->width;
      length = result->height;
      line_rect.width  = 1;
      line_rect.height = length;
    }

  line_buf = g_new0 (gfloat, length * 4);

  line_rect.x = result->x;
  line_rect.y = result->y;

  for (i = 0; i < size; i++)
    {
      gegl_buffer_get (input, &line_rect, 1.0, format, line_buf,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);

      gint start = 0;
      gint end = (length * 4) - 1;
      gboolean in_thresh = FALSE;
      for (j = 0; j < length * 4; j += 4)
        {
          gdouble key = get_key (&line_buf[j], mode);
          if (key >= o->threshold && !in_thresh)
            {
              start = j;
              in_thresh = TRUE;
            }
          else if (key < o->threshold && in_thresh)
            {
              end = j;
              in_thresh = FALSE;
            }
          if (in_thresh)
            {
              gint k = j;
              while (k > start)
                {
                  gdouble key1 = get_key (&line_buf[k], mode);
                  gdouble key2 = get_key (&line_buf[k - 4], mode);

                  gboolean in_place
                      = o->reverse ? (key2 > key1) : (key1 > key2);
                  if (in_place)
                    {
                      break;
                    }
                  gfloat r = line_buf[k];
                  gfloat g = line_buf[k + 1];
                  gfloat b = line_buf[k + 2];
                  gfloat a = line_buf[k + 3];
                  line_buf[k] = line_buf[k - 4];
                  line_buf[k + 1] = line_buf[k - 3];
                  line_buf[k + 2] = line_buf[k - 2];
                  line_buf[k + 3] = line_buf[k - 1];
                  line_buf[k - 4] = r;
                  line_buf[k - 3] = g;
                  line_buf[k - 2] = b;
                  line_buf[k - 1] = a;
                  k -= 4;
                }
            }
        }
      gegl_buffer_set (output, &line_rect, 0, format, line_buf,
                       GEGL_AUTO_ROWSTRIDE);

      if (o->direction == GEGL_ORIENTATION_HORIZONTAL)
        {
          line_rect.y++;
        }
      else
        {
          line_rect.x++;
        }
    }

  g_free(line_buf);

  return TRUE;
}

static gboolean
operation_process (GeglOperation        *operation,
                   GeglOperationContext *context,
                   const gchar          *output_prop,
                   const GeglRectangle  *result,
                   gint                  level)
{
  gboolean         success = FALSE;

  const GeglRectangle *in_rect
      = gegl_operation_source_get_bounding_box (operation, "input");

  if (in_rect && gegl_rectangle_is_infinite_plane (in_rect))
    {
      gpointer in = gegl_operation_context_get_object (context, "input");
      gegl_operation_context_take_object (context, "output",
                                          g_object_ref (G_OBJECT (in)));
      return TRUE;
    }
  else
    {
      GeglOperationFilterClass *klass;
      GeglBuffer *input;
      GeglBuffer *output;

      if (strcmp (output_prop, "output"))
        {
          g_warning ("requested processing of %s pad on a filter",
                     output_prop);
          return FALSE;
        }

      input
          = (GeglBuffer *)gegl_operation_context_dup_object (context, "input");
      output = gegl_operation_context_get_output_maybe_in_place (
          operation, context, input, result);
      klass = GEGL_OPERATION_FILTER_GET_CLASS (operation);
      success = klass->process (operation, input, output, result, level);

      g_clear_object (&input);
    }

  return success;
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationFilterClass *filter_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

  operation_class->prepare = prepare;
  operation_class->process = operation_process;
  operation_class->get_required_for_output = get_required_for_output;
  operation_class->get_cached_region = get_cached_region;
  filter_class->process    = process;

  gegl_operation_class_set_keys (operation_class,
    "name",        "gegl:pixelsort",
    "title",       "Pixel Sort",
    "categories",  "distort",
    "license",     "GPL3+",
    "description", "Sorts pixels by different properties within a threshold",
    NULL);
}

#endif
