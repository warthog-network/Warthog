#pragma once
#include "../webrtc/webrtc_sockaddr.hpp"
#include "tcp_sockaddr.hpp"

#ifdef DISABLE_LIBUV
#include "transport/ws/browser/ws_urladdr.hpp"
#endif

#include <variant>
struct Peerhost {
    using variant_t = std::variant<IP
#ifdef DISABLE_LIBUV
        ,
        std::string
#endif
        >;
    variant_t data;
    std::string to_string() const;
    Peerhost(IP ip)
        : data(std::move(ip))
    {
    }
#ifdef DISABLE_LIBUV
    Peerhost(std::string s)
        : data(std::move(s))
    {
    }
#endif
};

struct Peeraddr {
    [[nodiscard]] bool is_supported();
#ifndef DISABLE_LIBUV
    Peeraddr(TCPPeeraddr sa)
        : data { std::move(sa) }
    {
    }
    Peeraddr(WSPeeraddr sa)
        : data { std::move(sa) }
    {
    }
#else
    Peeraddr(WSUrladdr sa)
        : data { std::move(sa) }
    {
    }
#endif
    Peeraddr(WebRTCPeeraddr sa)
        : data { std::move(sa) }
    {
    }
    auto operator<=>(const Peeraddr&) const = default;
    using variant_t = std::variant<
#ifndef DISABLE_LIBUV
        TCPPeeraddr,
        WSPeeraddr,
#else
        WSUrladdr,
#endif
        WebRTCPeeraddr>;
    auto visit(auto lambda) const
    {
        return std::visit(lambda, data);
    }
    auto visit(auto lambda)
    {
        return std::visit(lambda, data);
    }
    Peerhost host() const;
    variant_t data;
    [[nodiscard]] std::optional<IP> ip() const;
    [[nodiscard]] uint16_t port() const;
    bool operator==(const Peeraddr&) const = default;
    std::string to_string() const;
    std::string_view type_str() const;
};
