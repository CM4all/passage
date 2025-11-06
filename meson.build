project('CM4all Passage', 'cpp', version: '0.28',
  meson_version: '>= 1.0',
  default_options : [
    'cpp_std=c++23',
    'warning_level=3',

    # This is only here to build subprojects as static libraries
    'default_library=static',

    'fmt:cpp_std=c++23',
  ],
)

version_h = configuration_data()
version_h.set_quoted('PACKAGE', meson.project_name())
version_h.set_quoted('VERSION', meson.project_version())
configure_file(output: 'version.h', configuration: version_h)

compiler = meson.get_compiler('cpp')

debug = get_option('b_ndebug') != 'true'

conf = configuration_data()

common_flags = [
  '-D_GNU_SOURCE',
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

libsystemd = dependency('libsystemd', required: get_option('systemd'))

inc = include_directories('src', 'libcommon/src', '.')

libcommon_enable_DefaultFifoBuffer = false
libcommon_require_curl = get_option('curl')
libcommon_require_sodium = get_option('sodium')

subdir('libcommon/src/util')
subdir('libcommon/src/co')
subdir('libcommon/src/time')
subdir('libcommon/src/lib/fmt')
subdir('libcommon/src/lib/sodium')
subdir('libcommon/src/lua')
subdir('libcommon/src/io')
subdir('libcommon/src/io/linux')
subdir('libcommon/src/system')
subdir('libcommon/src/event')
subdir('libcommon/src/net')
subdir('libcommon/src/net/control')
subdir('libcommon/src/net/linux')
subdir('libcommon/src/event/net')
subdir('libcommon/src/lib/curl')
subdir('libcommon/src/lua/io')
subdir('libcommon/src/lua/net')
subdir('libcommon/src/lua/event')

if libsystemd.found()
  subdir('libcommon/src/event/systemd')
  libsystemd = event_systemd_dep
endif

if sodium_dep.found()
  subdir('libcommon/src/lua/sodium')
else
  lua_sodium_dep = sodium_dep
endif

if curl_dep.found()
  subdir('libcommon/src/uri')
  subdir('libcommon/src/http')
else
  uri_dep = dependency('', required: false)
  http_dep = dependency('', required: false)
endif

libcommon_require_pg = get_option('pg')
subdir('libcommon/src/pg')
if pg_dep.found()
  subdir('libcommon/src/lua/pg')
else
  lua_pg_dep = pg_dep
endif

spawn = static_library('spawn',
  'libcommon/src/spawn/ZombieReaper.cxx',
  include_directories: inc,
  dependencies: [
  ])
spawn_dep = declare_dependency(link_with: spawn,
                               dependencies: [event_dep])

conf.set('HAVE_CURL', curl_dep.found())
conf.set('HAVE_LIBSODIUM', sodium_dep.found())
conf.set('HAVE_LIBSYSTEMD', libsystemd.found())
conf.set('HAVE_PG', pg_dep.found())
configure_file(output: 'config.h', configuration: conf)

lib = static_library(
  'libpassage',
  'src/Entity.cxx',
  'src/Parser.cxx',
  'src/Verify.cxx',
  include_directories: inc,
  dependencies: [
    util_dep,
  ],
)

lib_dep = declare_dependency(
  link_with: lib,
  dependencies: [
    util_dep,
  ]
)

executable('cm4all-passage',
  'src/system/SetupProcess.cxx',
  'src/LAction.cxx',
  'src/LResolver.cxx',
  'src/Instance.cxx',
  'src/Connection.cxx',
  'src/LRequest.cxx',
  'src/SendControl.cxx',
  'src/ExecPipe.cxx',
  'src/CommandLine.cxx',
  'src/Main.cxx',
  include_directories: inc,
  dependencies: [
    lib_dep,
    libsystemd,
    liblua,
    io_linux_dep,
    util_dep,
    coroutines_dep,
    lua_dep,
    lua_io_dep,
    lua_net_dep,
    lua_event_dep,
    lua_sodium_dep,
    lua_pg_dep,
    control_client_dep,
    event_net_dep,
    net_linux_dep,
    libsystemd,
    spawn_dep,
    curl_dep, uri_dep, http_dep,
    fmt_dep,
  ],
  install: true,
  install_dir: 'sbin',
)

executable('cm4all-passage-client',
  'src/Client.cxx',
  include_directories: inc,
  dependencies: [
    lib_dep,
    net_dep,
    util_dep,
    fmt_dep,
  ],
  install: true,
)

subdir('test')
subdir('libcommon/test/lua')
subdir('libcommon/test/net')
subdir('libcommon/test/pg')
subdir('libcommon/test/sodium')
subdir('libcommon/test/util')
subdir('doc')
