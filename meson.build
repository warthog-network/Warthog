project( 'Warthog', ['c','cpp'],
  version : '0.4.34',
  default_options : ['warning_level=3', 'cpp_std=c++20'])

json_dep = subproject('json', default_options : ['warning_level=0', 'werror=false']).get_variable('nlohmann_json_dep')

if not get_option('emscripten')
libuv_dep = subproject('libuv', default_options : ['warning_level=0', 'werror=false', 'build_tests=false']).get_variable('libuv_dep')
libwebsockets_dep = subproject('libwebsockets-4.3.3', default_options:['warning_level=0', 'werror=false','uv=enabled']).get_variable('libwebsockets_dep')
libjuice_dep = subproject('libjuice', default_options : ['warning_level=0', 'werror=false']).get_variable('libjuice_dep')
libmbedtls_dep = subproject('mbedtls', default_options : ['warning_level=0', 'werror=false']).get_variable('libmbedtls_dep')
usrsctp_dep = subproject('usrsctp', default_options : ['warning_level=0', 'werror=false']).get_variable('usrsctp_dep')
plog_dep = subproject('plog', default_options : ['warning_level=0', 'werror=false']).get_variable('plog_dep')
libdatachannel_dep = subproject('libdatachannel', default_options : ['warning_level=0', 'werror=false']).get_variable('libdatachannel_dep')
endif
sqlite3_dep = subproject('sqlite3',default_options : ['warning_level=0', 'werror=false']).get_variable('sqlite3_dep')

subdir('./thirdparty')
subdir('./src/shared')
subdir('./src/node')
subdir('./src/wallet')
subdir('./src/test')
