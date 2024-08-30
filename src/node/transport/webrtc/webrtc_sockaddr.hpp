#pragma once
#include "transport/helpers/sockaddr.hpp"
class Reader;

struct WebRTCPeeraddr: public Sockaddr {
    WebRTCPeeraddr(Reader& r);
    WebRTCPeeraddr(IP ip)
        :Sockaddr(ip,0){}
    std::string to_string() const;
    std::string to_string_with_protocol() const{
        return "udp+webrtc://" + to_string();
    }
    std::string_view type_str() const
    {
        return "WebRTC";
    }
    auto operator<=>(const WebRTCPeeraddr&) const = default;
    constexpr WebRTCPeeraddr(std::string_view);
};
