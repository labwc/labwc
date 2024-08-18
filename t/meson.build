test_lib_sources = files(
    '../src/common/buf.c',
    '../src/common/mem.c',
    '../src/common/string-helpers.c',
)

test_deps = [
  dep_cmocka,
]

tests = [
  'buf-simple',
]

if have_libyaml
  test_lib_sources += [
    '../src/common/yaml2xml.c',
  ]
  test_deps += [
    wlroots,
    yaml,
  ]
  tests += [
    'yaml2xml',
  ]
endif

test_lib = static_library(
  'test_lib',
  sources: test_lib_sources,
  include_directories: [labwc_inc],
  dependencies: test_deps
)

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
