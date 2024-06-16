#pragma once

#include "transport/helpers/tcp_sockaddr.hpp"
#include <vector>

class PeerServer;

namespace address_manager {
struct InitArg {
    PeerServer& peerServer;
#ifndef DISABLE_LIBUV
    const std::vector<TCPSockaddr>& pin;
#endif
};

}
