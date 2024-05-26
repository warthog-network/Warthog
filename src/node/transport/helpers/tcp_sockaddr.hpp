#pragma once

#include "transport/helpers/ip.hpp"
#include <stdexcept>
#ifndef DISABLE_LIBUV
struct sockaddr;
#endif

class Reader;
struct TCPSockaddrBase {
    TCPSockaddrBase(Reader& r);
    friend Writer& operator<<(Writer&, const TCPSockaddrBase&);
    static constexpr size_t byte_size() { return IPv4::byte_size() + sizeof(port); }
    constexpr TCPSockaddrBase(IPv4 ipv4, uint16_t port)
        : ip(ipv4)
        , port(port)
    {
    }
    constexpr TCPSockaddrBase(std::string_view);
    static TCPSockaddrBase from_sql_id(int64_t id)
    {
        return TCPSockaddrBase(
            IPv4(uint64_t(id & 0x0000FFFFFFFF0000) >> 16),
            uint16_t(0x000000000000FFFF & id));
    };
    int64_t to_sql_id()
    {
        return (int64_t(ip.data) << 16) + (int64_t(port));
    };
    auto operator<=>(const TCPSockaddrBase&) const = default;
    static constexpr std::optional<TCPSockaddrBase> parse(const std::string_view&);
#ifndef DISABLE_LIBUV
    operator sockaddr() const;
    sockaddr sock_addr() const;
#endif

    IPv4 ip;
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

std::optional<TCPSockaddrBase> constexpr TCPSockaddrBase::parse(const std::string_view& s)
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

    return TCPSockaddrBase { ip.value(), port.value() };
}

constexpr TCPSockaddrBase::TCPSockaddrBase(std::string_view s)
    : TCPSockaddrBase(
        [&] {
            auto ea { TCPSockaddrBase::parse(s) };
            if (ea)
                return *ea;
            throw std::runtime_error("Cannot parse endpoint address \"" + std::string(s) + "\".");
        }()) {};

struct TCPSockaddr : public TCPSockaddrBase {
    using TCPSockaddrBase::TCPSockaddrBase;
    TCPSockaddr(TCPSockaddrBase b)
        : TCPSockaddrBase(std::move(b))
    {
    }
    std::string to_string() const;
    static TCPSockaddr from_sql_id(int64_t id)
    {
        return { TCPSockaddrBase::from_sql_id(id) };
    }
    static constexpr std::optional<TCPSockaddr> parse(const std::string_view& sv)
    {
        auto p { TCPSockaddrBase::parse(sv) };
        if (p) {
            return TCPSockaddr(*p);
        }
        return {};
    }
};

struct WSSockaddr : public TCPSockaddrBase {
    using TCPSockaddrBase::TCPSockaddrBase;
    std::string to_string() const;
    WSSockaddr(TCPSockaddrBase b)
        : TCPSockaddrBase(std::move(b))
    {
    }
    static WSSockaddr from_sql_id(int64_t id)
    {
        return { TCPSockaddrBase::from_sql_id(id) };
    }
    static constexpr std::optional<WSSockaddr> parse(const std::string_view& sv)
    {
        auto p { TCPSockaddrBase::parse(sv) };
        if (p) {
            return WSSockaddr(*p);
        }
        return {};
    }
};
