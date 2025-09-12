gtest_compile_args = [
  '-Wno-undef',
]

if compiler.get_id() == 'gcc'
  gtest_compile_args += [
    '-Wno-suggest-attribute=format',
    '-Wno-suggest-attribute=noreturn',
    '-Wno-missing-declarations',
  ]
endif

gtest_main_dep = dependency('gtest', main: true, fallback: ['gtest', 'gtest_main_dep'])
gtest = declare_dependency(
  dependencies: [gtest_main_dep],
  compile_args: gtest_compile_args,
)

test(
  'TestParser',
  executable(
    'TestParser',
    'TestParser.cxx',
    '../src/Parser.cxx',
    include_directories: inc,
    install: false,
    dependencies: [
      lib_dep,
      gtest,
    ],
  ),
)
