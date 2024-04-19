#pragma once
#include "../tcp/tcp_sockaddr.hpp"
#include <variant>
struct Sockaddr {
    [[nodiscard]] bool is_supported();
    Sockaddr(TCPSockaddr socketAddr)
        : data { std::move(socketAddr) }
    {
    }
    auto operator<=>(const Sockaddr&) const = default;
    std::variant<TCPSockaddr> data;
    IPv4 ipv4() const;
    uint16_t port() const;
    bool operator==(const Sockaddr&) const = default;
    auto& get_tcp() const { return std::get<TCPSockaddr>(data); }
    auto& get_tcp() { return std::get<TCPSockaddr>(data); }
    std::string to_string() const;
};
