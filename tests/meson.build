lib_xml2 = library('libxml-2.0')
lib_wayland_server = library('wayland-server')
lib_xkbcommon = library('xkbcommon')
lib_glib = library('glib-2.0')

rcxml_lib = static_library(
  'rcxml',
  sources: files(
    '../src/config/rcxml.c',
    '../src/config/keybind.c',
    '../src/common/buf.c'
  ),
  dependencies: [xml2, wayland_server, xkbcommon, glib],
  include_directories: [labwc_inc],
  link_with: [lib_xml2, lib_wayland_server, lib_xkbcommon, lib_glib],
)

rcxml_tests = [
  't1000-rcxml-simple-parse.c',
  't1001-rcxml-nodenames-simple.c',
]

foreach t : rcxml_tests
  testname = t.split('.')[0].underscorify()
  exe = executable(
    testname,
    sources: [t, 'tap.c'],
    include_directories: [labwc_inc],
    link_with: [rcxml_lib],
  )
  test(testname, exe)
endforeach

