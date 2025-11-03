#pragma once

#include "transport/helpers/sockaddr.hpp"
#include <stdexcept>
#include <limits>
#ifndef DISABLE_LIBUV
struct sockaddr;
#endif

class Reader;

constexpr wrt::optional<uint16_t> parse_port(const std::string_view& s)
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
            if (port > std::numeric_limits<uint16_t>::max())
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

wrt::optional<Sockaddr4> constexpr Sockaddr4::parse(const std::string_view& s)
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

    return Sockaddr4 { ip.value(), port.value() };
}

constexpr Sockaddr4::Sockaddr4(std::string_view s)
    : Sockaddr4(
        [&] {
            auto ea { Sockaddr4::parse(s) };
            if (ea)
                return *ea;
            throw std::runtime_error("Cannot parse endpoint address \"" + std::string(s) + "\".");
        }()) {};

struct TCPPeeraddr : public Sockaddr4 {
    using Sockaddr4::Sockaddr4;
    TCPPeeraddr(Sockaddr4 b)
        : Sockaddr4(std::move(b))
    {
    }
    std::string to_string() const;
    std::string to_string_with_protocol() const{
        return "tcp://" + to_string();
    }
    std::string_view type_str() const
    {
        return "TCP";
    }
    static TCPPeeraddr from_sql_id(int64_t id)
    {
        return { Sockaddr4::from_sql_id(id) };
    }
    [[nodiscard]] static constexpr wrt::optional<TCPPeeraddr> parse(const std::string_view& sv)
    {
        auto p { Sockaddr4::parse(sv) };
        if (p) {
            return TCPPeeraddr(*p);
        }
        return {};
    }
};

struct WSPeeraddr: public Sockaddr  {
    using Sockaddr::Sockaddr;
    auto operator<=>(const WSPeeraddr&) const = default;
    std::string to_string() const;
    std::string to_string_with_protocol() const{
        return "ws://" + to_string();
    }
    WSPeeraddr(Sockaddr addr):Sockaddr(std::move(addr)){}
    std::string_view type_str() const
    {
        return "WS";
    }
    // static WSSockaddr from_sql_id(int64_t id)
    // {
    //     return { TCPSockaddrBase::from_sql_id(id) };
    // }
    // static constexpr wrt::optional<WSSockaddr> parse(const std::string_view& sv)
    // {
    //     auto p { TCPSockaddrBase::parse(sv) };
    //     if (p) {
    //         return WSSockaddr(*p);
    //     }
    //     return {};
    // }
};
