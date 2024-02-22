
sources = [
  'uvw/async.cpp',
  'uvw/check.cpp',
  'uvw/dns.cpp',
  'uvw/emitter.cpp',
  'uvw/fs.cpp',
  'uvw/fs_event.cpp',
  'uvw/fs_poll.cpp',
  'uvw/idle.cpp',
  'uvw/lib.cpp',
  'uvw/loop.cpp',
  'uvw/pipe.cpp',
  'uvw/poll.cpp',
  'uvw/prepare.cpp',
  'uvw/process.cpp',
  'uvw/signal.cpp',
  'uvw/stream.cpp',
  'uvw/tcp.cpp',
  'uvw/thread.cpp',
  'uvw/timer.cpp',
  'uvw/tty.cpp',
  'uvw/udp.cpp',
  'uvw/util.cpp',
  'uvw/work.cpp',
]

uvw_lib = library(
  'uvw', sources,
  include_directories: '.',
  dependencies: [libuv_dep],
  cpp_args: ['-DUVW_AS_LIB'],
  install: true,
)

uvw_dep = declare_dependency(
  include_directories: ['.'],
  dependencies: [libuv_dep],
  link_with: [uvw_lib],
)
