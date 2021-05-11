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

property_boolean (order, "Reverse", FALSE)
     description ("Reverse sort order")

property_seed (seed, "Random seed", rand)

#else

#define GEGL_OP_AREA_FILTER
#define GEGL_OP_NAME     pixelsort
#define GEGL_OP_C_SOURCE pixelsort.c

#include "gegl-op.h"

static void
prepare (GeglOperation *operation)
{
  const Babl              *format;
  format = gegl_operation_get_source_format (operation, "input");

  gegl_operation_set_format (operation, "input",  format);
  gegl_operation_set_format (operation, "output", format);
}


static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result,
         gint                 level)
  {
  GeglProperties    *o = GEGL_PROPERTIES (operation);
  gint           size, i, pos;
  GeglRectangle  dst_rect;


  if (o->direction == GEGL_ORIENTATION_HORIZONTAL)
    {
      size = result->height;
      dst_rect.width  = result->width;
      dst_rect.height = 1;
      pos = result->y;
    }
  else
    {
      size = result->width;
      dst_rect.width  = 1;
      dst_rect.height = result->height;
      pos = result->x;
    }

  dst_rect.x = result->x;
  dst_rect.y = result->y;

  for (i = 0; i < size; i++)
    {
      GeglRectangle src_rect;
      // gint shift = gegl_random_int_range (o->rand, i + pos, 0, 0, 0,
      //                                     -o->shift, o->shift + 1);
      gint shift = 0;

      if (o->direction == GEGL_ORIENTATION_HORIZONTAL)
        {
          dst_rect.y = i + result->y;
          src_rect = dst_rect;
          src_rect.x = result->x + shift;
        }
      else
        {
          dst_rect.x = i + result->x;
          src_rect = dst_rect;
          src_rect.y = result->y + shift;
        }


      gegl_buffer_copy (input, &src_rect, GEGL_ABYSS_CLAMP,
                        output, &dst_rect);
    }

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
