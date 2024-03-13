#pragma once

#include "net/ipv4.hpp"
#include <stdexcept>
#include <uv.h>

class Reader;
struct EndpointAddress {
    EndpointAddress(Reader& r);
    constexpr EndpointAddress(IPv4 ipv4, uint16_t port)
        : ipv4(ipv4)
        , port(port)
    {
    }
    constexpr EndpointAddress(std::string_view);
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
    static constexpr std::optional<EndpointAddress> parse(const std::string_view&);
    operator sockaddr() const { return sock_addr(); }
    std::string to_string() const;
    sockaddr sock_addr() const;

    IPv4 ipv4;
    uint16_t port = 0;
};

constexpr std::optional<uint16_t> parse_port(const std::string_view& s)
{
    uint16_t out;
    if (s.size() > 5)
        return {};
    uint32_t port = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9')
            return {};
        uint8_t digit = s[i] - '0';
        if (i == 5) {
            if (port > 6553)
                return {};
            if (digit >= 6)
                return {};
        }
        port *= 10;
        port += digit;
    }
    out = port;
    return out;
}

std::optional<EndpointAddress> constexpr EndpointAddress::parse(const std::string_view& s)
{
    size_t d1 = s.find(":");
    auto ipv4str { s.substr(0, d1) };
    auto ip = IPv4::parse(ipv4str);
    if (!ip)
        return {};
    if (d1 == std::string::npos)
        return {};
    size_t d2 = s.find("//", d1 + 1);
    auto portstr { s.substr(d1 + 1, d2 - d1 - 1) };
    auto port = parse_port(portstr);
    if (!port)
        return {};

    return EndpointAddress { ip.value(), port.value() };
}

constexpr EndpointAddress::EndpointAddress(std::string_view s)
    : EndpointAddress(
        [&] {
            auto ea { EndpointAddress::parse(s) };
            if (ea)
                return *ea;
            throw std::runtime_error("Cannot parse endpoint address \"" + std::string(s) + "\".");
        }()) {};
