project('CM4all Passage', 'cpp', version: '0.3',
  default_options : ['cpp_std=c++14'])

compiler = meson.get_compiler('cpp')

# TODO: use get_option('b_ndebug'), but that doesn't work with Meson 0.37.1
if get_option('buildtype') == 'debug'
  debug = true
else
  debug = false
endif

common_flags = [
  '-D_GNU_SOURCE',
  '-Wall',
  '-Wextra',
  '-Wwrite-strings', '-Wcast-qual', '-Wcast-align', '-Wfloat-equal',
  '-Wshadow', '-Wpointer-arith', '-Wsign-compare',
  '-Wmissing-declarations', '-Wmissing-noreturn', '-Wmissing-format-attribute',
  '-Wredundant-decls', '-Wno-long-long', '-Wundef',
  '-Wunused',
]

if debug
  common_flags += ['-DPOISON']
endif

add_global_arguments(common_flags,
  '-Wno-non-virtual-dtor',
  language: 'cpp')

boost = dependency('boost', modules: ['program_options'], version: '>= 1.54')
liblua = dependency('luajit')
libevent = dependency('libevent', version: '>= 2.0.19')
libsystemd = dependency('libsystemd')
libbp = dependency('libcm4all-beng-proxy')

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

util = static_library('util',
  'libcommon/src/util/AllocatedString.cxx',
  'libcommon/src/util/Exception.cxx',
  'libcommon/src/util/LeakDetector.cxx',
  'libcommon/src/util/PrintException.cxx',
  'libcommon/src/util/StringBuilder.cxx',
  'libcommon/src/util/StringParser.cxx',
  'libcommon/src/util/StringUtil.cxx',
  'libcommon/src/util/StringView.cxx',
  include_directories: inc,
  dependencies: [
  ])
util_dep = declare_dependency(link_with: util)

lua = static_library('lua',
  'libcommon/src/lua/Error.cxx',
  'libcommon/src/lua/RunFile.cxx',
  'libcommon/src/lua/State.cxx',
  dependencies: [
    liblua,
  ]
)
lua_dep = declare_dependency(link_with: lua)

event = static_library('event',
  'libcommon/src/event/Loop.cxx',
  'libcommon/src/event/ShutdownListener.cxx',
  'libcommon/src/event/CleanupTimer.cxx',
  'libcommon/src/event/DeferEvent.cxx',
  'libcommon/src/event/SignalEvent.cxx',
  include_directories: inc,
  dependencies: [
    libevent,
    util_dep,
  ])
event_dep = declare_dependency(link_with: event)

io = static_library('io',
  'libcommon/src/io/FileDescriptor.cxx',
  'libcommon/src/io/WriteBuffer.cxx',
  'libcommon/src/io/MultiWriteBuffer.cxx',
  'libcommon/src/io/Logger.cxx',
  include_directories: inc,
  dependencies: [
  ])
io_dep = declare_dependency(link_with: io)

net = static_library('net',
  'libcommon/src/net/SocketAddress.cxx',
  'libcommon/src/net/StaticSocketAddress.cxx',
  'libcommon/src/net/AllocatedSocketAddress.cxx',
  'libcommon/src/net/MaskedSocketAddress.cxx',
  'libcommon/src/net/IPv4Address.cxx',
  'libcommon/src/net/IPv6Address.cxx',
  'libcommon/src/net/HostParser.cxx',
  'libcommon/src/net/AddressInfo.cxx',
  'libcommon/src/net/Resolver.cxx',
  'libcommon/src/net/Parser.cxx',
  'libcommon/src/net/ToString.cxx',
  'libcommon/src/net/SocketDescriptor.cxx',
  'libcommon/src/net/UniqueSocketDescriptor.cxx',
  'libcommon/src/net/djb/NetstringInput.cxx',
  'libcommon/src/net/djb/NetstringHeader.cxx',
  'libcommon/src/net/djb/NetstringGenerator.cxx',
  include_directories: inc,
  dependencies: [
    event_dep,
    io_dep,
  ])
net_dep = declare_dependency(link_with: net)

event_net = static_library('event_net',
  'libcommon/src/event/net/ConnectSocket.cxx',
  'libcommon/src/event/net/ServerSocket.cxx',
  'libcommon/src/event/net/UdpListener.cxx',
  'libcommon/src/event/net/djb/NetstringServer.cxx',
  'libcommon/src/event/net/djb/NetstringClient.cxx',
  include_directories: inc,
  dependencies: [
    libevent,
    event_dep,
    net_dep,
    util_dep,
  ])
event_net_dep = declare_dependency(link_with: event_net)

spawn = static_library('spawn',
  'libcommon/src/spawn/Registry.cxx',
  include_directories: inc,
  dependencies: [
  ])
spawn_dep = declare_dependency(link_with: spawn,
                               dependencies: [event_dep])

executable('cm4all-passage',
  'src/CommandLine.cxx',
  'src/system/SetupProcess.cxx',
  'src/LAction.cxx',
  'src/LAddress.cxx',
  'src/LResolver.cxx',
  'src/CgroupProc.cxx',
  'src/MountProc.cxx',
  'src/Instance.cxx',
  'src/Connection.cxx',
  'src/Request.cxx',
  'src/LRequest.cxx',
  'src/FadeChildren.cxx',
  'src/ExecPipe.cxx',
  'src/Main.cxx',
  include_directories: inc,
  dependencies: [
    libbp,
    boost, libevent, libsystemd,
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
  include_directories: inc,
  dependencies: [
    net_dep,
    util_dep,
  ],
  install: true,
)

subdir('libcommon/test/util')
subdir('libcommon/test/net')
subdir('doc')
