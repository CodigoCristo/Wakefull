project('wakefull', 'c',
  version : '1.0.0',
  default_options : [
    'warning_level=2',
    'c_std=c11',
    'buildtype=release'
  ]
)

# Add feature test macros
add_project_arguments('-D_POSIX_C_SOURCE=200809L', language : 'c')
add_project_arguments('-D_DEFAULT_SOURCE', language : 'c')

# Find dependencies
x11_dep = dependency('x11', required : true)

# System functions will be checked at compile time

# Create the executable
wakefull_exe = executable('wakefull',
  'wakefull.c',
  dependencies : [x11_dep],
  install : true,
  install_dir : get_option('bindir')
)

# Summary
summary({
  'prefix': get_option('prefix'),
  'bindir': get_option('bindir'),
}, section: 'Directories')

summary({
  'X11': x11_dep.found(),
  'C Standard': get_option('c_std'),
  'Build Type': get_option('buildtype'),
}, section: 'Dependencies')
