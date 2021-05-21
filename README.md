pixelsort
=========

A custom GEGL operation (and by extension GIMP filter) that implements a pixel
sorting effect.  To use it click "Tools->GEGL Operation" and choose "Pixel Sort"
in the dropdown.

This is primarily a port of the following processing script to a GEGL operation:
https://github.com/kimasendorf/ASDFPixelSort

## Compiling and Installing

### Linux

To compile and install you will need the GEGL header files (`libgegl-dev` on
Debian based distributions or `gegl` on Arch Linux) and meson (`meson` on
most distributions).

```bash
meson setup --buildtype=release build
ninja -C build
cp build/pixelsort.so ~/.local/share/gegl-0.4/plug-ins
```

If you have an older version of gegl you may need to copy to `~/.local/share/gegl-0.3/plug-ins`
instead (on Ubuntu 18.04 for example).

**NOTE:** as of writing this pixelsort seems to not work with gegl-0.3.  It works
when run with the GEGL command line utility but for some reason none of the operation's
properties show up in the GEGL tool window.

### Windows

The easiest way to compile this project on Windows is by using msys2.  Download
and install it from here: https://www.msys2.org/

Open a msys2 terminal with `C:\msys64\mingw64.exe`.  Run the following to
install required build dependencies:

```bash
pacman --noconfirm -S base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-meson mingw-w64-x86_64-gegl
```

Then build the same way you would on Linux:

```bash
meson setup --buildtype=release build
ninja -C build
```

To install it, copy the resulting DLL (`build/pixelsort.dll`) to the folder
where GIMP finds GEGL operations:

```bash
cp build/pixelsort.dll '/c/Program Files/GIMP 2/lib/gegl-0.4/
```

Or if you already have a pre-built `pixelsort.dll` just copy it to
`C:\Program Files\GIMP 2\lib\gegl-0.4\ `.

## Example Images

Original image:

![Original image](examples/example_1_original.png)

Vertical sorting with luminance as the mode:

![Original image](examples/example_1_luminance.png)

Horizontal sorting with white level as the mode:

![Original image](examples/example_1_white.png)

Complete vertical sorting by luminance (threshold of 0):

![Original image](examples/example_1_complete_sort.png)
