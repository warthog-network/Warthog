#pragma once

#include "general/params.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <uv.h>

class Reader;

struct IPv4 {
    IPv4(Reader& r);
    IPv4(const sockaddr_in& sin);
    IPv4(uint32_t data = 0)
        : data(data)
    {
    }
    bool is_valid(bool allowLocalhost = true) const;
    auto operator<=>(const IPv4& rhs) const = default;
    static std::optional<IPv4> parse(const std::string_view&);
    uint32_t data;
    std::string to_string() const;
};

std::optional<uint16_t> parse_port(const std::string_view&);
struct EndpointAddress {
    EndpointAddress(Reader& r);
    EndpointAddress(IPv4 ipv4, uint16_t port = DEFAULT_ENDPOINT_PORT)
        : ipv4(ipv4)
        , port(port)
    {
    }
    static EndpointAddress from_sql_id(int64_t id)
    {
        return EndpointAddress(
            IPv4(uint64_t(id & 0x0000FFFFFFFF0000) >> 16),
            uint16_t(0x000000000000FFFF & id));
    };
    int64_t to_sql_id()
    {
        return (int64_t(ipv4.data) << 16) + (int64_t(port));
    };
    auto operator<=>(const EndpointAddress&) const = default;
    EndpointAddress() {};
    static std::optional<EndpointAddress> parse(const std::string_view&);
    std::string to_string() const;
    sockaddr_in sock_addr() const;

    IPv4 ipv4;
    uint16_t port = 0;
};
