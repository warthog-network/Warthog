optlib = static_library('mylib', 
  ['./verus_clhash_opt.cpp'],
  cpp_args: ['-mpclmul', '-msse4', '-msse4.1', '-msse4.2', '-mssse3', '-mavx', '-maes', '-O3'],
  dependencies: [libuv_dep],
  include_directories : [include_spdlog, include_secp256k1, include_wh,include_json, include_httplib],
)

exe = executable('verustest', 
  [ src_wh, 
  './main.cpp'
  ],
  dependencies: [libuv_dep],
  include_directories : [include_spdlog, include_secp256k1, include_wh,include_json, include_httplib],
  link_with: [libsecp256k1, optlib] ,
  install : true)
