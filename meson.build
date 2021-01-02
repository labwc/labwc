project(
  'labwc',
  'c',
  license: 'GPL-2',
  default_options: [
    'c_std=c11',
    'warning_level=2',
  ],
)

add_project_arguments(
  [
    '-DWLR_USE_UNSTABLE',
    '-Wno-enum-compare',
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
  wlroots_conf = wlroots_proj.get_variable('conf_data')
  wlroots_has_xwayland = wlroots_conf.get('WLR_HAS_XWAYLAND') == 1
else
  wlroots       = dependency('wlroots', version: '>= 0.11.0')
  wlroots_has_xwayland = cc.get_define('WLR_HAS_XWAYLAND', prefix: '#include <wlr/config.h>', dependencies: wlroots) == '1'
endif
wayland_server  = dependency('wayland-server')
wayland_protos  = dependency('wayland-protocols')
xkbcommon       = dependency('xkbcommon')
xcb             = dependency('xcb', required: get_option('xwayland'))
xml2            = dependency('libxml-2.0')
glib            = dependency('glib-2.0')
cairo           = dependency('cairo')
pangocairo      = dependency('pangocairo')
input           = dependency('libinput', version: '>=1.14')

if get_option('xwayland').enabled() and not wlroots_has_xwayland
	error('no wlroots Xwayland support')
endif
have_xwayland = xcb.found() and wlroots_has_xwayland
conf_data = configuration_data()
conf_data.set10('HAVE_XWAYLAND', have_xwayland)

labwc_inc       = include_directories('include')

subdir('protocols')

labwc_deps      = [
  server_protos, wayland_server, wlroots, xkbcommon, xml2, glib,
  cairo, pangocairo, input
]

subdir('include')
subdir('src')
subdir('docs')

executable(
  meson.project_name(),
  labwc_sources,
  include_directories: [labwc_inc],
  dependencies: labwc_deps,
  install: true,
)
