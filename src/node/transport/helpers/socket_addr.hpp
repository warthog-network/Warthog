#pragma once
#include "../webrtc/webrtc_sockaddr.hpp"
#include "tcp_sockaddr.hpp"
#include <variant>
struct Sockaddr {
    [[nodiscard]] bool is_supported();
#ifndef DISABLE_LIBUV
    Sockaddr(TCPSockaddr sa)
        : data { std::move(sa) }
    {
    }
#else
#endif
    Sockaddr(WebRTCSockaddr sa)
        : data { std::move(sa) }
    {
    }
    Sockaddr(WSSockaddr sa)
        : data { std::move(sa) }
    {
    }
    auto operator<=>(const Sockaddr&) const = default;
    using variant_t = std::variant<
#ifndef DISABLE_LIBUV
        TCPSockaddr,
#endif
        WSSockaddr,
        WebRTCSockaddr>;
    auto visit(auto lambda) const
    {
        return std::visit(lambda, data);
    }
    auto visit(auto lambda)
    {
        return std::visit(lambda, data);
    }
    variant_t data;
    [[nodiscard]] IP ip() const;
    [[nodiscard]] uint16_t port() const;
    bool operator==(const Sockaddr&) const = default;
    std::string to_string() const;
    std::string_view type_str() const;
};
