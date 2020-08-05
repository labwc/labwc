project(
  'labwc',
  'c',
  license: 'GPL-2',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'werror=true',
  ],
)

add_project_arguments(
  [
    '-DWLR_USE_UNSTABLE',
  ],
  language: 'c',
)

cc = meson.get_compiler('c')

add_project_arguments(cc.get_supported_arguments(
  [
    '-Wno-unused-parameter',
    '-Wundef',
  ]),
  language: 'c',
)

wlroots_proj = subproject(
  'wlroots',
  default_options: ['examples=false'],
  required: false,
)

if wlroots_proj.found()
  wlroots       = wlroots_proj.get_variable('wlroots')
else
  wlroots       = dependency('wlroots', version: '>= 0.10.0')
endif
wayland_server  = dependency('wayland-server')
wayland_protos  = dependency('wayland-protocols')
xkbcommon       = dependency('xkbcommon')
xml2            = dependency('libxml-2.0')
glib            = dependency('glib-2.0')
cairo           = dependency('cairo')
pangocairo      = dependency('pangocairo')

labwc_inc       = include_directories('include')

subdir('protocols')

labwc_deps      = [
  server_protos, wayland_server, wlroots, xkbcommon, xml2, glib,
  cairo, pangocairo
]

subdir('src')
subdir('tests')
subdir('docs')

executable(
  meson.project_name(),
  labwc_sources,
  include_directories: [labwc_inc],
  dependencies: labwc_deps,
  install: true,
)
