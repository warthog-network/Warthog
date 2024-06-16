#include "webrtc_sockaddr.hpp"

std::string WebRTCSockaddr::to_string() const
{
    return "udp+webrtc://" + _ip.to_string() + ":" + std::to_string(_port);
}
