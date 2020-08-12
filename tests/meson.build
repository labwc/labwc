rcxml_lib = static_library(
  'rcxml',
  sources: files(
    '../src/config/rcxml.c',
    '../src/config/keybind.c',
    '../src/common/dir.c',
    '../src/common/buf.c',
    '../src/common/font.c',
    '../src/common/log.c',
  ),
  dependencies: labwc_deps,
  include_directories: [labwc_inc],
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
    dependencies: [wlroots],
    include_directories: [labwc_inc],
    link_with: [rcxml_lib],
  )
  test(testname, exe)
endforeach

