include_usockets=include_directories( [ './src',])

libusockets =  library('usockets', [ 
  './src/eventing/libuv.c',
  './src/bsd.c',
  './src/context.c',
  './src/loop.c',
  './src/socket.c',
  ],
  c_args: ['-DLIBUS_USE_LIBUV','-DLIBUS_NO_SSL', '-w'],
  cpp_args: ['-DLIBUS_USE_LIBUV','-DLIBUS_NO_SSL'],
  dependencies: libuv_dep,
  include_directories : ['./src'],
  install: true
  )

