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

property_boolean (reverse_order, "Reverse sort order", FALSE)
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
get_key (gfloat pixel[4], GeglPixelsortMode mode)
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

static inline void
swap_rgba_pixels(gfloat (*pixels)[4], gint a, gint b)
{
  gfloat temp_r = pixels[a][0];
  gfloat temp_g = pixels[a][1];
  gfloat temp_b = pixels[a][2];
  gfloat temp_a = pixels[a][3];
  pixels[a][0] = pixels[b][0];
  pixels[a][1] = pixels[b][1];
  pixels[a][2] = pixels[b][2];
  pixels[a][3] = pixels[b][3];
  pixels[b][0] = temp_r;
  pixels[b][1] = temp_g;
  pixels[b][2] = temp_b;
  pixels[b][3] = temp_a;
}

static void
merge (gfloat (*in)[4], gint left, gint right, gint end, gfloat (*out)[4],
       gboolean reverse, GeglPixelsortMode mode)
{
  gint i = left;
  gint j = right;
  gboolean comp;
  for (gint k = left; k < end; k++)
    {
      comp = reverse ^ (get_key (in[i], mode) <= get_key (in[j], mode));
      if (i < right && (j >= end || comp))
        {
          out[k][0] = in[i][0];
          out[k][1] = in[i][1];
          out[k][2] = in[i][2];
          out[k][3] = in[i][3];
          i++;
        }
      else
        {
          out[k][0] = in[j][0];
          out[k][1] = in[j][1];
          out[k][2] = in[j][2];
          out[k][3] = in[j][3];
          j++;
        }
    }
}

static void
stable_sort (gfloat (*pixels)[4], gfloat (*work)[4], gint start, gint end,
             gboolean reverse, GeglPixelsortMode mode)
{
  gint n = (end - start) + 1;

  for (gint width = 1; width < n; width *= 2)
    {
      for (gint i = start; i < end; i += width * 2)
        {
          merge (pixels, i, MIN (i + width, end), MIN (i + width * 2, end),
                 work, reverse, mode);
        }
      for (gint i = start; i < end; i++)
        {
          pixels[i][0] = work[i][0];
          pixels[i][1] = work[i][1];
          pixels[i][2] = work[i][2];
          pixels[i][3] = work[i][3];
        }
    }
}

static void
prepare (GeglOperation *operation)
{
#if GEGL_MAJOR_VERSION >= 4
  const Babl *space = gegl_operation_get_source_space (operation, "input");

  gegl_operation_set_format (operation, "input", babl_format_with_space ("RGBA float", space));
  gegl_operation_set_format (operation, "output", babl_format_with_space ("RGBA float", space));
#else
  gegl_operation_set_format (operation, "input",  babl_format ("RGBA float"));
  gegl_operation_set_format (operation, "output", babl_format ("RGBA float"));
#endif
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
  gint              num_lines, length, line_num, j;
  GeglRectangle     line_rect;

  if (o->direction == GEGL_ORIENTATION_HORIZONTAL)
    {
      num_lines = result->height;
      length = result->width;
      line_rect.width  = length;
      line_rect.height = 1;
    }
  else
    {
      num_lines = result->width;
      length = result->height;
      line_rect.width  = 1;
      line_rect.height = length;
    }

  gfloat (*line_buf)[4] = (gfloat (*)[4]) g_new (gfloat, length * 4);
  gfloat (*work_buf)[4] = (gfloat (*)[4]) g_new (gfloat, length * 4);

  line_rect.x = result->x;
  line_rect.y = result->y;

  gdouble threshold = o->threshold / 10;

  for (line_num = 0; line_num < num_lines; line_num++)
    {
      gegl_buffer_get (input, &line_rect, 1.0, format, (gfloat *) line_buf,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);

      gint start = 0;
      gboolean in_thresh = FALSE;
      for (j = 0; j < length; j++)
        {
          gdouble key = get_key (line_buf[j], mode);
          if (key >= threshold && !in_thresh)
            {
              start = j;
              in_thresh = TRUE;
            }
          else if (((key < threshold) || j == length - 1)  && in_thresh)
            {
              stable_sort (line_buf, work_buf, start, j, o->reverse_order,
                           o->mode);
              in_thresh = FALSE;
            }
        }
      
      gegl_buffer_set (output, &line_rect, 0, format, (gfloat *) line_buf,
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
  g_free(work_buf);

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
