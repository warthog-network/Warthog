src_libuv = [
  './api/http/endpoint.cpp',
  './eventloop/address_manager/tcp_connections.cpp',
  './transport/tcp/conman.cpp',
  './transport/tcp/connect_request.cpp',
  './transport/tcp/connection.cpp',
  './transport/ws/native/ws_conman.cpp',
  './transport/ws/native/connection.cpp',
  './transport/ws/native/ws_session.cpp',
  ]

src_browser = [
  './api/wasm/api.cpp',
  './config/browser.cpp',
  './eventloop/address_manager/websocket_outbound_schedule.cpp',
  './transport/ws/browser/connect_request.cpp',
  './transport/ws/browser/connection.cpp',
  './transport/ws/browser/ws_urladdr.cpp',
  ]

src= [
  './api/http/json.cpp',
  './api/http/parse.cpp',
  './api/interface.cpp',
  './api/realtime.cpp',
  './api/stratum/stratum_server.cpp',
  './api/types/all.cpp',
  './block/body/generator.cpp',
  './block/body/primitives.cpp',
  './block/chain/consensus_headers.cpp',
  './block/chain/fork_range.cpp',
  './block/chain/header_chain.cpp',
  './block/chain/history/history.cpp',
  './block/chain/pin.cpp',
  './block/chain/range.cpp',
  './block/chain/signed_snapshot.cpp',
  './block/chain/state.cpp',
  './block/header/batch.cpp',
  './block/header/shared_batch.cpp',
  './block/header/timestamprule.cpp',
  './chainserver/account_cache.cpp',
  './chainserver/db/chain_db.cpp',
  './chainserver/mining_subscription.cpp',
  './chainserver/server.cpp',
  './chainserver/state/helpers/consensus.cpp',
  './chainserver/state/helpers/past_chains.cpp',
  './chainserver/state/state.cpp',
  './chainserver/state/transactions/apply_stage.cpp',
  './chainserver/state/transactions/block_applier.cpp',
  './cmdline/cmdline.cpp',
  './communication/buffers/recvbuffer.cpp',
  './communication/buffers/sndbuffer.cpp',
  './communication/message_elements/helper_types.cpp',
  './communication/messages.cpp',
  './config/config.cpp',
  './eventloop/address_manager/address_manager.cpp',
  './eventloop/address_manager/flat_address_set.cpp',
  './eventloop/chain_cache.cpp',
  './eventloop/eventloop.cpp',
  './eventloop/peer_chain.cpp',
  './eventloop/sync/block_download/attorney.cpp',
  './eventloop/sync/block_download/block_download.cpp',
  './eventloop/sync/block_download/connection_data.cpp',
  './eventloop/sync/block_download/focus.cpp',
  './eventloop/sync/block_download/forks.cpp',
  './eventloop/sync/block_download/stage_state.cpp',
  './eventloop/sync/header_download/connection_data.cpp',
  './eventloop/sync/header_download/header_download.cpp',
  './eventloop/sync/header_download/probe_balanced.cpp',
  './eventloop/timer.cpp',
  './eventloop/timer_element.cpp',
  './eventloop/types/chainstate.cpp',
  './eventloop/types/conndata.cpp',
  './eventloop/types/rtc/peer_rtc_state.cpp',
  './eventloop/types/rtc/rtc_state.cpp',
  './general/descriptor.cpp',
  './global/globals.cpp',
  './mempool/mempool.cpp',
  './mempool/subscription.cpp',
  './mempool/txmap.cpp',
  './peerserver/ban_cache.cpp',
  './peerserver/db/peer_db.cpp',
  './peerserver/peerserver.cpp',
  './transport/connection_base.cpp',
  './transport/helpers/ip.cpp',
  './transport/helpers/ipv4.cpp',
  './transport/helpers/ipv6.cpp',
  './transport/helpers/per_ip_counter.cpp',
  './transport/helpers/socket_addr.cpp',
  './transport/helpers/tcp_sockaddr.cpp',
  './transport/webrtc/connection.cpp',
  './transport/webrtc/sdp_util.cpp',
  './transport/webrtc/webrtc_sockaddr.cpp',
  src_sqlitecpp,
  src_wh,
]


deps = [json_dep, libdatachannel_dep, sqlite3_dep]
if get_option('emscripten')
  src = [src, src_browser]
  include_thirdparty=[include_trezorcrypto,include_wh,include_secp256k1,include_sqlitecpp, include_spdlog,include_tomlplusplus, include_tl]
  lib_thirdparty=[libsecp256k1]
else
  src = [src, src_libuv]
  deps =  [deps, sqlite3_dep,  libuv_dep, uvw_dep, libwebsockets_dep, libmbedtls_dep ]
  include_thirdparty=[include_trezorcrypto,include_wh,include_secp256k1,include_sqlitecpp, include_spdlog,include_usockets,include_uwebsockets,include_tomlplusplus, include_tl]
  lib_thirdparty=[libsecp256k1, libusockets]
endif


if get_option('emscripten')

link_args = [
    '-lwebsocket.js',
    '-sNO_DISABLE_EXCEPTION_CATCHING', '-sSTACK_SIZE=4mb', '-sMAXIMUM_MEMORY=768mb', '-fexceptions', '-sPROXY_TO_PTHREAD', '-sWASMFS',
    '-sUSE_CLOSURE_COMPILER=0', '-sIMPORTED_MEMORY',
    '-sWASM_BIGINT=1', '-sMODULARIZE', '-sDYNAMIC_EXECUTION=0', '-sNO_POLYFILL',
    '-sSTRICT_JS=0', '-sALLOW_MEMORY_GROWTH', '-sINITIAL_MEMORY=8388608', '-sGLOBAL_BASE=4096', '-sALLOW_TABLE_GROWTH',
    '-sERROR_ON_UNDEFINED_SYMBOLS=1', '-sENVIRONMENT=web,worker',
    '-sWASMFS', '-sPTHREAD_POOL_SIZE=1', '-sLLD_REPORT_UNDEFINED', '-sEXPORT_ES6', '-sUSE_ES6_IMPORT_META',
  ]
foreach file : files('./api/wasm/api.js')
    link_args +=[  '--js-library='+ file.full_path()]
endforeach

executable('wart-node', vcs_dep, [src,'./main.cpp', src_spdlog],
  include_directories:['./' ,include_thirdparty],
  link_with: lib_thirdparty,
  dependencies: deps,
  cpp_args: ['-fexceptions'],
  link_args: link_args,
  install : true)

websocket = executable('websocket', 'websocket.cpp',
  cpp_args: ['-fexceptions'],
  link_args:['-fexceptions', '-sPROXY_TO_PTHREAD', '-sWASMFS',
  '-pthread',
  '-lwebsocket.js',
    '-sUSE_CLOSURE_COMPILER=0', '-sIMPORTED_MEMORY', 
  '-sWASM_BIGINT=1', '-sMODULARIZE', '-sDYNAMIC_EXECUTION=0', '-sNO_POLYFILL', 
  '-sSTRICT_JS=0', '-sALLOW_MEMORY_GROWTH', '-sINITIAL_MEMORY=16777216', '-sSTACK_SIZE=512KB', '-sGLOBAL_BASE=4096', '-sALLOW_TABLE_GROWTH', 
  '-sERROR_ON_UNDEFINED_SYMBOLS=1', '-sENVIRONMENT=web,worker', 
  '-sWASMFS', '-sPTHREAD_POOL_SIZE=1', '-sLLD_REPORT_UNDEFINED', '-sEXPORT_ES6', '-sUSE_ES6_IMPORT_META',
    ],
  name_suffix: 'html',
  install : true)
else
executable('client', vcs_dep, [src,'./rtc_client.cpp', src_spdlog,
  ],
  include_directories:['./' ,include_thirdparty,
    ],
  link_with: lib_thirdparty,
  dependencies: deps,
  install : true)
executable('wart-node', vcs_dep, [src,'./main.cpp', src_spdlog],
  include_directories:['./' ,include_thirdparty],
  link_with: lib_thirdparty,
  dependencies: deps,
  install : true)
endif


