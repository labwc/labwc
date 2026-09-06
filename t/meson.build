test_deps = [
  dep_cmocka,
  input,
  glib,
  xml2,
  wlroots,
]

test_lib = static_library(
  'test_lib',
  sources: files(
    '../src/config/accel.c',
    '../src/common/parse-double.c',
    '../src/common/buf.c',
    '../src/common/mem.c',
    '../src/common/string-helpers.c',
    '../src/common/xml.c',
    '../src/common/parse-bool.c',
  ),
  include_directories: [labwc_inc],
  dependencies: test_deps,
)

tests = [
  'accel',
  'buf-simple',
  'str',
  'xml',
]

foreach t : tests
  test(
    'test_@0@'.format(t),
    executable(
      'test_@0@'.format(t),
      sources: '@0@.c'.format(t),
      include_directories: [labwc_inc],
      link_with: [test_lib],
      dependencies: test_deps,
    ),
    is_parallel: false,
  )
endforeach
