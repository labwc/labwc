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

add_project_arguments(cc.get_supported_arguments([
	'-Wundef',
	'-Wlogical-op',
	'-Wmissing-include-dirs',
	'-Wold-style-definition',
	'-Wpointer-arith',
	'-Winit-self',
	'-Wstrict-prototypes',
	'-Wimplicit-fallthrough=2',
	'-Wendif-labels',
	'-Wstrict-aliasing=2',
	'-Woverflow',
	'-Wmissing-prototypes',
	'-Wno-missing-braces',
	'-Wno-missing-field-initializers',
	'-Wno-unused-parameter',
]), language: 'c')

wlroots_proj = subproject(
	'wlroots',
	default_options: ['examples=false'],
	required: false,
)
if wlroots_proj.found()
	wlroots = wlroots_proj.get_variable('wlroots')
else
	wlroots = dependency('wlroots', version: '>= 0.10.0')
endif

wayland_protos = dependency('wayland-protocols')
wayland_server = dependency('wayland-server')
xkbcommon      = dependency('xkbcommon')

subdir('protocols')

labwc_sources = [
  'main.c',
  'server.c',
  'output.c',
  'view.c',
  'xdg.c',
  'xwl.c',
  'deco.c',
  'dbg.c',
]

labwc_headers = [
  'labwc.h',
]

executable(
  meson.project_name(),
  labwc_sources + labwc_headers,
  dependencies: [
    server_protos,
    wayland_server,
    wlroots,
    xkbcommon,
  ],
  install: true,
)
