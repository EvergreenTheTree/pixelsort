pixelsort
=========

A custom GEGL operation (and by extension GIMP filter) that implements a pixel
sorting effect.  To use it click "Tools->GEGL Operation" and choose "Pixel Sort"
in the dropdown.

This is primarily a port of the following processing script to a GEGL operation:
https://github.com/kimasendorf/ASDFPixelSort

## Compiling and Installing

To compile and install you will need the GEGL header files (`libgegl-dev` on
Debian based distributions or `gegl` on Arch Linux).

```bash
meson setup build
meson compile -C build
cp build/pixelsort.so ~/.local/share/gegl-0.4/plugins
```

## Example Images

Original image:

![Original image](examples/example_1_original.png)

Vertical sorting with luminance as the mode:

![Original image](examples/example_1_luminance.png)

Horizontal sorting with white level as the mode:

![Original image](examples/example_1_white.png)

Complete vertical sorting by luminance (threshold of 0):

![Original image](examples/example_1_complete_sort.png)
