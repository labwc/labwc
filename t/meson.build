test_lib = static_library(
  'test_lib',
  sources: files(
    '../src/common/buf.c',
    '../src/common/mem.c',
    '../src/common/string-helpers.c'
  ),
  include_directories: [labwc_inc],
  dependencies: [dep_cmocka],
)

tests = [
  'buf-simple',
]

foreach t : tests
  test(
    'test_@0@'.format(t),
    executable(
      'test_@0@'.format(t),
      sources: '@0@.c'.format(t),
      include_directories: [labwc_inc],
      link_with: [test_lib],
    ),
    is_parallel: false,
  )
endforeach
