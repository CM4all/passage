project('CM4all Passage', 'cpp', version: '0.14',
  meson_version: '>= 0.56',
  default_options : [
    'cpp_std=c++2a',
    'warning_level=3',
  ],
)

compiler = meson.get_compiler('cpp')

debug = get_option('b_ndebug') != 'true'

common_flags = [
  '-D_GNU_SOURCE',

  # disable boost specific code in libcommon
  '-DNO_BOOST',
]

test_common_flags = [
  '-Wcast-align',
  '-Wcast-qual',
  '-Wdouble-promotion',
  '-Wfloat-equal',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-noreturn',
  '-Wredundant-decls',
  '-Wshadow',
  '-Wundef',
  '-Wunused',
  '-Wvla',
  '-Wwrite-strings',

  # clang specific warning options:
  '-Wunreachable-code-aggressive',
  '-Wused-but-marked-unused',
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

  '-Wcomma-subscript',
  '-Wextra-semi',
  '-Wmismatched-tags',
  '-Woverloaded-virtual',
  '-Wsign-promo',
  '-Wvolatile',
  '-Wvirtual-inheritance',

  '-Wno-missing-field-initializers',

  # a vtable without a dtor is just fine
  '-Wno-non-virtual-dtor',

  # clang specific warning options:
  '-Wcomma',
  '-Wheader-hygiene',
  '-Winconsistent-missing-destructor-override',
]

add_global_arguments(common_flags, language: 'cpp')
add_global_arguments(compiler.get_supported_arguments(test_global_cxxflags), language: 'cpp')
add_project_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')

libsystemd = dependency('libsystemd')

inc = include_directories('src', 'libcommon/src')

libcommon_enable_boost = false
libcommon_enable_DefaultFifoBuffer = false

subdir('libcommon/src/util')
subdir('libcommon/src/lib/fmt')
subdir('libcommon/src/lua')
subdir('libcommon/src/io')
subdir('libcommon/src/io/linux')
subdir('libcommon/src/system')
subdir('libcommon/src/event')
subdir('libcommon/src/net')
subdir('libcommon/src/net/control')
subdir('libcommon/src/event/net')
subdir('libcommon/src/lua/net')

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
  'src/LResolver.cxx',
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
    libsystemd,
    liblua,
    io_linux_dep,
    util_dep,
    lua_dep,
    lua_net_dep,
    control_client_dep,
    event_net_dep,
    spawn_dep,
    fmt_dep,
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
    fmt_dep,
  ],
  install: true,
)

subdir('test')
subdir('libcommon/test/util')
subdir('libcommon/test/net')
subdir('doc')
