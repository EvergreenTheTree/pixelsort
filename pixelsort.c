#include <libgimp/gimp.h>
#include <stdlib.h>

static void query(void);
static void run(const gchar *name,
                gint nparams,
                const GimpParam *param,
                gint *nreturn_vals,
                GimpParam **return_vals);
static void pixelsort(GimpDrawable *drawable);

GimpPlugInInfo PLUG_IN_INFO = {
  NULL,
  NULL,
  query,
  run
};

MAIN()

static void query(void) {
  static GimpParamDef args[] = {
    {
      GIMP_PDB_INT32,
      "run-mode",
      "Run mode"
    },
    {
      GIMP_PDB_IMAGE,
      "image",
      "Input image"
    },
    {
      GIMP_PDB_DRAWABLE,
      "drawable",
      "Input drawable"
    }
  };

  gimp_install_procedure(
    "pixelsort",
    "Sorts pixels",
    "Sorts pixels in a glitchy looking way.",
    "Evergreen",
    "Copyright Evergreen",
    "2021",
    "Pixel Sort",
    "RGB*, GRAY*",
    GIMP_PLUGIN,
    G_N_ELEMENTS(args),
    0,
    args, NULL
  );

  gimp_plugin_menu_register("pixelsort", "<Image>/Filters/Misc");
}

static void run(const gchar *name,
                gint nparams,
                const GimpParam *param,
                gint *nreturn_vals,
                GimpParam **return_vals) {
  static GimpParam values[1];
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  GimpRunMode run_mode;
  GimpDrawable *drawable;

  /* Setting mandatory output values */
  *nreturn_vals = 1;
  *return_vals = values;

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

  /* Getting run_mode - we won't display a dialog if 
   * we are in NONINTERACTIVE mode */
  run_mode = param[0].data.d_int32;

  /* Get the specified drawable */
  drawable = gimp_drawable_get(param[2].data.d_drawable);

  gimp_progress_init("Pixel Sort...");
  
  pixelsort(drawable);

  gimp_displays_flush();
  gimp_drawable_detach(drawable);
}

static void sortRGBA(guchar *pixels, gint len) {
  // for (gint i = 0; i < (len * 4); i += 4) {
  //   pixels[i] = pixels[i + 2];
  //   pixels[i + 1] = pixels[i + 2];
  // }
  for (gint i = 0; i < (len * 4); i += 4) {
    int j = i;
    while (j > 0) {
      gint pixel1[4] = {pixels[j], pixels[j + 1], pixels[j + 2], pixels[j + 3]};
      gdouble lum1 = 0.2126 * pixel1[0] + 0.7152 * pixel1[1] + 0.0722 * pixel1[2];
      gint pixel2[4] = {pixels[j - 4], pixels[j - 3], pixels[j - 2], pixels[j - 1]};
      gdouble lum2 = 0.2126 * pixel2[0] + 0.7152 * pixel2[1] + 0.0722 * pixel2[2];

      if (lum2 > lum1) {
        break;
      }
        pixels[j - 4] = pixel1[0];
        pixels[j - 3] = pixel1[1];
        pixels[j - 2] = pixel1[2];
        pixels[j - 1] = pixel1[3];
        pixels[j] = pixel2[0];
        pixels[j + 1] = pixel2[1];
        pixels[j + 2] = pixel2[2];
        pixels[j + 3] = pixel2[3];
      j -= 4;
    }
  }
}

static void pixelsort(GimpDrawable *drawable) {
  gint channels;
  gint x1, y1, x2, y2;
  GimpPixelRgn rgn_in, rgn_out;
  guchar *row;

  gimp_drawable_mask_bounds(drawable->drawable_id, &x1, &y1, &x2, &y2);
  channels = gimp_drawable_bpp(drawable->drawable_id);

  gimp_pixel_rgn_init(
    &rgn_in,
    drawable,
    x1, y1,
    x2 - x1, y2 - y1, 
    FALSE, FALSE
  );
  gimp_pixel_rgn_init(
    &rgn_out,
    drawable,
    x1, y1,
    x2 - x1, y2 - y1, 
    TRUE, TRUE
  );

  row = g_new(guchar, channels * (x2 - x1));
  for (gint i = y1; i < y2; i++) {
    gimp_pixel_rgn_get_row(&rgn_in, row, x1, MAX(y1, i), x2 - x1);
    sortRGBA(row, x2 - x1);
    gimp_pixel_rgn_set_row(&rgn_out, row, x1, i, x2 - x1);
    if (i % 10 == 0) {
      gimp_progress_update((gdouble) (i - y1) / (gdouble) (y2 - y1));
    }
  }

  g_free(row);

  gimp_drawable_flush(drawable);
  gimp_drawable_merge_shadow(drawable->drawable_id, TRUE);
  gimp_drawable_update(drawable->drawable_id, x1, y1, x2 - x1, y2 - y1);
}