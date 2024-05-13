project('datachannelwasm',
    'cpp',
    version : '0.2.4',
    license : 'MIT',
)

sources=[
  './wasm/src/candidate.cpp',
  './wasm/src/channel.cpp',
  './wasm/src/configuration.cpp',
  './wasm/src/description.cpp',
  './wasm/src/datachannel.cpp',
  './wasm/src/peerconnection.cpp',
  ]

link_args = []
foreach file : files('wasm/js/webrtc.js')
    link_args +=[  file.full_path()]
endforeach

libdatachannelwasm = library(
  'libdatachannelwasm',
  sources,
  dependencies: [],
  link_args: link_args,
  include_directories: ['./wasm/include/rtc/']
)


libdatachannelwasm_dep = declare_dependency(
    include_directories: ['./wasm/include'],
    link_args:  [join_paths(meson.current_source_dir(),'wasm/js/webrtc.js')],
    link_with: [libdatachannelwasm]
)
