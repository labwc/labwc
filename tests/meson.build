rcxml_lib = static_library(
  'rcxml',
  sources: files('../src/config/rcxml.c', '../src/common/buf.c'),
  dependencies: xml2,
  include_directories: [labwc_inc],
  link_with: library('libxml-2.0'),
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
    link_with: rcxml_lib,
  )
  test(testname, exe)
endforeach

