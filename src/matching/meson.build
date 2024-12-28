
json = subproject('json')
json_dep = json.get_variable('nlohmann_json_dep')

if get_option('emscripten')
exe = executable('wasm_matching_demo', ['wasm_matching_demo.cpp',src_defi],
  dependencies: json_dep,
  cpp_args:['-fexceptions',],
  include_directories: include_wh,
  link_args: ['-sMODULARIZE', '-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,stringToNewUTF8', '-sEXPORT_ES6',
    '-sNO_DISABLE_EXCEPTION_CATCHING', 
    '-sINITIAL_MEMORY=67108864'
    ],
  install : true)
else
exe = executable('defi', ['defi.cpp',src_defi],
  include_directories: include_wh,
  dependencies: json_dep,
  install : true)
endif

