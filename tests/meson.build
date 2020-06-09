rcxml_lib = static_library(
  'rcxml',
  sources: files('../src/config/rcxml.c', '../src/common/buf.c'),
  dependencies: xml2,
  include_directories: [labwc_inc],
  link_with: library('libxml-2.0'),
)

t1000 = executable(
  't1000-rcxml-simple-parse',
  sources: ['t1000-rcxml-simple-parse.c', 'tap.c'],
  include_directories: [labwc_inc],
  link_with: rcxml_lib,
)
test('t1000', t1000)

t1001 = executable(
  't1001-rcxml-nodenames-simple',
  sources: ['t1001-rcxml-nodenames-simple.c', 'tap.c'],
  include_directories: [labwc_inc],
  link_with: rcxml_lib,
)
test('t1001', t1001)
