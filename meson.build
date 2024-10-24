project(
  'labwc',
  'c',
  version: '0.8.0',
  license: 'GPL-2.0-only',
  meson_version: '>=0.59.0',
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
  '-Walloca',
  '-Wunused-macros',

  '-Wno-unused-parameter',
  '-Wno-expansion-to-defined',
]), language: 'c')

version='"@0@"'.format(meson.project_version())
git = find_program('git', native: true, required: false)
if git.found()
  git_commit = run_command([git, 'describe', '--dirty'], check: false)
  if git_commit.returncode() == 0
    version = '"@0@"'.format(git_commit.stdout().strip())
  endif
endif
add_project_arguments('-DLABWC_VERSION=@0@'.format(version), language: 'c')

wlroots = dependency(
  'wlroots-0.18',
  default_options: ['default_library=static', 'examples=false'],
  version: ['>=0.18.0', '<0.19.0'],
)

wlroots_has_xwayland = wlroots.get_variable('have_xwayland') == 'true'
have_libsfdo = not get_option('icon').disabled()

wayland_server = dependency('wayland-server', version: '>=1.19.0')
wayland_protos = dependency('wayland-protocols', version: '>=1.35')
xkbcommon = dependency('xkbcommon')
xcb = dependency('xcb', required: get_option('xwayland'))
xcb_icccm = dependency('xcb-icccm', required: get_option('xwayland'))
drm_full = dependency('libdrm')
drm = drm_full.partial_dependency(compile_args: true, includes: true)
xml2 = dependency('libxml-2.0')
glib = dependency('glib-2.0')
cairo = dependency('cairo')
pangocairo = dependency('pangocairo')
input = dependency('libinput', version: '>=1.14')
pixman = dependency('pixman-1')
math = cc.find_library('m')
png = dependency('libpng')
svg = dependency('librsvg-2.0', version: '>=2.46', required: false)
sfdo_basedir = dependency(
  'libsfdo-basedir',
  default_options: ['default_library=static', 'examples=false', 'tests=false'],
  version: '>=0.1.3',
  required: have_libsfdo,
)
sfdo_desktop = dependency(
  'libsfdo-desktop',
  default_options: ['default_library=static', 'examples=false', 'tests=false'],
  version: '>=0.1.3',
  required: have_libsfdo,
)
sfdo_icon = dependency(
  'libsfdo-icon',
  default_options: ['default_library=static', 'examples=false', 'tests=false'],
  version: '>=0.1.3',
  required: have_libsfdo,
)

if get_option('xwayland').enabled() and not wlroots_has_xwayland
	error('no wlroots Xwayland support')
endif
have_xwayland = xcb.found() and wlroots_has_xwayland
conf_data = configuration_data()
conf_data.set10('HAVE_XWAYLAND', have_xwayland)

if get_option('svg').disabled()
  have_rsvg = false
else
  have_rsvg = svg.found()
endif
conf_data.set10('HAVE_RSVG', have_rsvg)

conf_data.set10('HAVE_LIBSFDO', have_libsfdo)

if get_option('static_analyzer').enabled()
  add_project_arguments(['-fanalyzer'], language: 'c')
endif

msgfmt = find_program('msgfmt', required: get_option('nls'))
if msgfmt.found()
  source_root = meson.current_source_dir()
  conf_data.set('HAVE_NLS', 1)
  subdir('po')
else
  conf_data.set('HAVE_NLS', 0)
endif

labwc_inc = include_directories('include')

subdir('protocols')

labwc_deps = [
  server_protos,
  wayland_server,
  wlroots,
  xkbcommon,
  xcb_icccm,
  xml2,
  glib,
  cairo,
  drm,
  pangocairo,
  input,
  pixman,
  math,
  png,
]
if have_rsvg
  labwc_deps += [
    svg,
  ]
endif
if have_libsfdo
  labwc_deps += [
    sfdo_basedir,
    sfdo_desktop,
    sfdo_icon,
  ]
endif

subdir('include')
subdir('src')
subdir('docs')

dep_cmocka = dependency('cmocka', required: get_option('test'))
if dep_cmocka.found()
  subdir('t')
endif

executable(
  meson.project_name(),
  labwc_sources,
  include_directories: [labwc_inc],
  dependencies: labwc_deps,
  install: true,
)

install_data('data/labwc.desktop', install_dir: get_option('datadir') / 'wayland-sessions')

install_data('data/labwc-portals.conf', install_dir: get_option('datadir') / 'xdg-desktop-portal')

icons = ['labwc-symbolic.svg', 'labwc.svg']
foreach icon : icons
  icon_path = join_paths('data', icon)
  install_data(icon_path, install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps')
endforeach
