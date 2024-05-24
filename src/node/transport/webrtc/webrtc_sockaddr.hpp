#pragma once
class Reader;
#include "transport/helpers/ip.hpp"

struct WebRTCSockaddr {
    WebRTCSockaddr(Reader& r);
    WebRTCSockaddr(IP ip, uint16_t port = 0)
        : ip(ip), port(port)
    {
    }
    std::string to_string() const;
    auto operator<=>(const WebRTCSockaddr&) const = default;
    constexpr WebRTCSockaddr(std::string_view);
    IP ip;
    uint16_t port;
};
