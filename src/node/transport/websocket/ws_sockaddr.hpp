#pragma once
#include "transport/helpers/ipv4.hpp"
class Reader;
struct WSSockaddr {
    WSSockaddr(Reader& r);
    constexpr WSSockaddr(IPv4 ipv4, uint16_t port)
        : ipv4(ipv4)
        , port(port)
    {
    }
    constexpr WSSockaddr(std::string_view);
    static WSSockaddr from_sql_id(int64_t id)
    {
        return WSSockaddr(
            IPv4(uint64_t(id & 0x0000FFFFFFFF0000) >> 16),
            uint16_t(0x000000000000FFFF & id));
    };
    int64_t to_sql_id()
    {
        return (int64_t(ipv4.data) << 16) + (int64_t(port));
    };
    auto operator<=>(const WSSockaddr&) const = default;
    static constexpr std::optional<WSSockaddr> parse(const std::string_view&);
    operator sockaddr() const { return sock_addr(); }
    std::string to_string() const;
    sockaddr sock_addr() const;

    IPv4 ipv4;
    uint16_t port = 0;
};
