if not get_option('emscripten')
deps = libuv_dep
else
deps = []
endif

executable('wart-wallet', vcs_dep, 
  [
    './api_call.cpp', 
    './cmdline/cmdline.cpp', 
    './main.cpp', 
    src_wh, 
    src_spdlog
],
include_directories : [include_secp256k1,include_wh,include_json, include_httplib, include_spdlog],
link_with: libsecp256k1, 
dependencies: deps,
install : true)
