project('pixelsort', 'c',
  version : '0.1',
  license : 'GPL-3.0-or-later')

# These arguments are only used to build the shared library
# not the executables that use the library.
lib_args = ['-DBUILDING_PIXELSORT']

gegl = dependency('gegl-0.4')
shlib = shared_library('pixelsort', 'pixelsort.c',
  c_args : lib_args,
  gnu_symbol_visibility : 'hidden',
  dependencies : gegl,
  name_prefix : '',
)

# Make this library usable as a Meson subproject.
pixelsort_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with : shlib)

