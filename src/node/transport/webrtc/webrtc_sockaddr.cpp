#include "webrtc_sockaddr.hpp"

std::string WebRTCPeeraddr::to_string() const
{
    return ip.to_string() + ":" + std::to_string(port);
}
