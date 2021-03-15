project(
  'labwc',
  'c',
  version: '0.1.0',
  license: 'GPL-2',
  default_options: [
    'c_std=c11',
    'warning_level=2',
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

version='"@0@"'.format(meson.project_version())
add_project_arguments('-DLABWC_VERSION=@0@'.format(version), language: 'c')

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
  wlroots       = dependency('wlroots', version: '>= 0.13.0')
  wlroots_has_xwayland = cc.get_define('WLR_HAS_XWAYLAND', prefix: '#include <wlr/config.h>', dependencies: wlroots) == '1'
endif
wayland_server  = dependency('wayland-server')
wayland_protos  = dependency('wayland-protocols')
xkbcommon       = dependency('xkbcommon')
xcb             = dependency('xcb', required: get_option('xwayland'))
drm_full        = dependency('libdrm')
drm             = drm_full.partial_dependency(compile_args: true, includes: true)
xml2            = dependency('libxml-2.0')
glib            = dependency('glib-2.0')
cairo           = dependency('cairo')
pangocairo      = dependency('pangocairo')
input           = dependency('libinput', version: '>=1.14')
pixman          = dependency('pixman-1')
math            = cc.find_library('m')

if get_option('xwayland').enabled() and not wlroots_has_xwayland
	error('no wlroots Xwayland support')
endif
have_xwayland = xcb.found() and wlroots_has_xwayland
conf_data = configuration_data()
conf_data.set10('HAVE_XWAYLAND', have_xwayland)

labwc_inc       = include_directories('include')

subdir('protocols')

labwc_deps      = [
  server_protos,
  wayland_server,
  wlroots,
  xkbcommon,
  xml2,
  glib,
  cairo,
  drm,
  pangocairo,
  input,
  pixman,
  math,
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
