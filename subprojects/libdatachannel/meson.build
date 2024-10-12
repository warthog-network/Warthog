project('libdatachannel',
    'cpp',
    version : '0.20.1',
    license : 'Apache-2.0/GPL-2.0-or-later ',
)

sources = files(
	'src/candidate.cpp',
	'src/channel.cpp',
	'src/configuration.cpp',
	'src/datachannel.cpp',
	'src/description.cpp',
	'src/mediahandler.cpp',
	'src/global.cpp',
	'src/message.cpp',
	'src/peerconnection.cpp',
	'src/rtcpreceivingsession.cpp',
	'src/track.cpp',
	'src/websocket.cpp',
	'src/websocketserver.cpp',
	'src/rtppacketizationconfig.cpp',
	'src/rtcpsrreporter.cpp',
	'src/rtppacketizer.cpp',
	'src/h264rtppacketizer.cpp',
	'src/h264rtpdepacketizer.cpp',
	'src/nalunit.cpp',
	'src/h265rtppacketizer.cpp',
	'src/h265nalunit.cpp',
	'src/av1rtppacketizer.cpp',
	'src/nalunit.cpp',
	'src/rtcpnackresponder.cpp',
	'src/rtp.cpp',
	'src/capi.cpp',
	'src/plihandler.cpp',
	'src/impl/certificate.cpp',
	'src/impl/channel.cpp',
	'src/impl/datachannel.cpp',
	'src/impl/dtlssrtptransport.cpp',
	'src/impl/dtlstransport.cpp',
	'src/impl/icetransport.cpp',
	'src/impl/init.cpp',
	'src/impl/peerconnection.cpp',
	'src/impl/logcounter.cpp',
	'src/impl/sctptransport.cpp',
	'src/impl/threadpool.cpp',
	'src/impl/tls.cpp',
	'src/impl/track.cpp',
	'src/impl/utils.cpp',
	'src/impl/processor.cpp',
	'src/impl/sha.cpp',
	'src/impl/pollinterrupter.cpp',
	'src/impl/pollservice.cpp',
	'src/impl/http.cpp',
	'src/impl/httpproxytransport.cpp',
	'src/impl/tcpserver.cpp',
	'src/impl/tcptransport.cpp',
	'src/impl/tlstransport.cpp',
	'src/impl/transport.cpp',
	'src/impl/verifiedtlstransport.cpp',
	'src/impl/websocket.cpp',
	'src/impl/websocketserver.cpp',
	'src/impl/wstransport.cpp',
	'src/impl/wshandshake.cpp')

plog_dep = subproject('plog', default_options : ['warning_level=0', 'werror=false']).get_variable('plog_dep')
libjuice_dep = subproject('libjuice', default_options : ['warning_level=0', 'werror=false']).get_variable('libjuice_dep')
usrsctp_dep = subproject('usrsctp', default_options : ['warning_level=0', 'werror=false']).get_variable('usrsctp_dep')
libmbedtls_dep = subproject('mbedtls', default_options : ['warning_level=0', 'werror=false']).get_variable('libmbedtls_dep')



inc_dirs = include_directories('./include/')
libdatachannel = library(
  'libdatachannel',
  sources,
  cpp_args: ['-DMBEDTLS_SSL_DTLS_SRTP', '-DUSE_MBEDTLS',  '-DRTC_ENABLE_MEDIA=0', '-DRTC_ENABLE_WEBSOCKET=0', '-DRTC_EXPORTS', '-DJUICE_STATIC'],
  dependencies: [
    plog_dep, 
    libmbedtls_dep,
    usrsctp_dep,
    libjuice_dep],
  include_directories: ['./src', './include/', './include/rtc']
)


winlink = []
if host_machine.system() == 'windows'
  winlink = ['-lbcrypt']
endif

libdatachannel_dep = declare_dependency(
    include_directories: inc_dirs,
    link_with: libdatachannel,
    link_args: winlink
)
