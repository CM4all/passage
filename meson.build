project('CM4all Passage', 'cpp', version: '0.12',
  meson_version: '>= 0.47',
  default_options : [
    'cpp_std=c++2a',
    'warning_level=3',
  ],
)

compiler = meson.get_compiler('cpp')

debug = get_option('b_ndebug') != 'true'

common_flags = [
  '-D_GNU_SOURCE',
]

test_common_flags = [
  '-Wcast-align',
  '-Wcast-qual',
  '-Wfloat-equal',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-noreturn',
  '-Wredundant-decls',
  '-Wshadow',
  '-Wundef',
  '-Wunused',
  '-Wwrite-strings',

  '-Wno-long-long',
]

test_global_common_flags = [
  '-fvisibility=hidden',
]

add_global_link_arguments(
  compiler.get_supported_link_arguments(
    # make relocations read-only (hardening)
    '-Wl,-z,relro',

    # no lazy binding, please - not worth it for a daemon
    '-Wl,-z,now',
  ),
  language: 'cpp'
)

if not debug
  test_global_common_flags += [
    '-ffunction-sections',
    '-fdata-sections',
  ]

  add_global_link_arguments(
    compiler.get_supported_link_arguments(
      '-Wl,--gc-sections',
      '-Wl,--icf=all',
    ),
    language: 'cpp'
  )
endif

test_global_cxxflags = test_global_common_flags + [
]

test_cxxflags = test_common_flags + [
  '-fno-threadsafe-statics',
  '-fmerge-all-constants',

  '-Wno-non-virtual-dtor',
]

add_global_arguments(common_flags, language: 'cpp')
add_global_arguments(compiler.get_supported_arguments(test_global_cxxflags), language: 'cpp')
add_project_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')

libsystemd = dependency('libsystemd')
libbp = dependency('libcm4all-beng-proxy3', version: '>= 15.7')

gtest_compile_args = [
  '-Wno-undef',
]

if compiler.get_id() == 'gcc'
  gtest_compile_args += [
    '-Wno-suggest-attribute=format',
    '-Wno-suggest-attribute=noreturn',
    '-Wno-missing-declarations',

    # needed on Jessie for gtest's IsNullLiteralHelper
    '-Wno-conversion-null',
  ]
endif

gtest = declare_dependency(
  dependencies: [dependency('gtest', main: true)],
  compile_args: gtest_compile_args,
)

inc = include_directories('src', 'libcommon/src')

libcommon_enable_c = false
libcommon_enable_boost = false
libcommon_enable_DefaultFifoBuffer = false
libcommon_enable_net_log = false

subdir('libcommon/src/util')
subdir('libcommon/src/lua')
subdir('libcommon/src/io')
subdir('libcommon/src/system')
subdir('libcommon/src/event')
subdir('libcommon/src/net')
subdir('libcommon/src/event/net')

spawn = static_library('spawn',
  'libcommon/src/spawn/ZombieReaper.cxx',
  include_directories: inc,
  dependencies: [
  ])
spawn_dep = declare_dependency(link_with: spawn,
                               dependencies: [event_dep])

executable('cm4all-passage',
  'src/system/SetupProcess.cxx',
  'src/LAction.cxx',
  'src/LAddress.cxx',
  'src/LResolver.cxx',
  'src/CgroupProc.cxx',
  'src/MountProc.cxx',
  'src/Instance.cxx',
  'src/Connection.cxx',
  'src/Parser.cxx',
  'src/Entity.cxx',
  'src/LRequest.cxx',
  'src/FadeChildren.cxx',
  'src/ExecPipe.cxx',
  'src/CommandLine.cxx',
  'src/Main.cxx',
  include_directories: inc,
  dependencies: [
    libbp,
    libsystemd,
    liblua,
    util_dep,
    lua_dep,
    event_net_dep,
    spawn_dep,
  ],
  install: true,
  install_dir: 'sbin',
)

executable('cm4all-passage-client',
  'src/Client.cxx',
  'src/Parser.cxx',
  'src/Entity.cxx',
  include_directories: inc,
  dependencies: [
    net_dep,
    util_dep,
  ],
  install: true,
)

subdir('libcommon/test/util')
subdir('libcommon/test/net')
subdir('test')
subdir('doc')
