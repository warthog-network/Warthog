#pragma once
class Reader;
#include "transport/helpers/ip.hpp"

struct WebRTCSockaddr {
    WebRTCSockaddr(Reader& r);
    WebRTCSockaddr(IP ip, uint16_t port = 0)
        : _ip(ip)
        , _port(port)
    {
    }
    auto port() const { return _port; }
    std::string to_string() const;
    std::string_view type_str() const
    {
        return "WebRTC";
    }
    auto operator<=>(const WebRTCSockaddr&) const = default;
    constexpr WebRTCSockaddr(std::string_view);
    IP ip() const { return _ip; }
    IP _ip;
    uint16_t _port;
};
