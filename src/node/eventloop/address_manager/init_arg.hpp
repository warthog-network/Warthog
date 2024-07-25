#pragma once

#ifndef DISABLE_LIBUV
#include "transport/helpers/tcp_sockaddr.hpp"
#else
#include "transport/ws/browser/ws_urladdr.hpp"
#endif
#include <vector>

class PeerServer;

namespace address_manager {
struct InitArg {
    PeerServer& peerServer;
#ifndef DISABLE_LIBUV
    const std::vector<TCPPeeraddr>& pin;
#else
    const std::vector<WSUrladdr>& pin;
#endif
};

}
