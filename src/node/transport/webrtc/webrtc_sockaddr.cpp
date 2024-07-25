#include "webrtc_sockaddr.hpp"

std::string WebRTCPeeraddr::to_string() const
{
    return "udp+webrtc://" + ip.to_string() + ":" + std::to_string(port);
}
