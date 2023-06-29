executable('wart-miner', vcs_dep, 
  [
    './api_call.cpp', 
    './cmdline/cmdline.cpp', 
    './third_party/cpuminer-opt/sph_sha2.c',
    './third_party/cpuminer-opt/sha256-hash-opt.c',
    './main.cpp', 
    './worker.cpp', 
    './mine.cpp', 
    src_wh, 
    src_spdlog
    ],
  c_args: ['-march=native'],
  include_directories : [include_spdlog, include_secp256k1, include_wh,include_json, include_httplib],
  link_with: libsecp256k1, 
  dependencies: [libuv_dep],
  install : true)
