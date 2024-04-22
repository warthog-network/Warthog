#pragma once
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
    Sockaddr(WSSockaddr sa)
        : data { std::move(sa) }
    {
    }
#endif
    auto operator<=>(const Sockaddr&) const = default;
    using variant_t = std::variant<
#ifndef DISABLE_LIBUV
        TCPSockaddr
#else
        WSSockaddr
#endif
        >;
    variant_t data;
    IPv4 ipv4() const;
    uint16_t port() const;
    bool operator==(const Sockaddr&) const = default;
#ifndef DISABLE_LIBUV
    auto& get_tcp() const
    {
        return std::get<TCPSockaddr>(data);
    }
    auto& get_tcp() { return std::get<TCPSockaddr>(data); }
#endif
    std::string to_string() const;
};
