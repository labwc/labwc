rcxml_lib = static_library(
  'rcxml',
  sources: ['../src/config/rcxml.c'],
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
