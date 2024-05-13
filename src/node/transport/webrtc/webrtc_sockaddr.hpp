#pragma once

struct WebRTCSockaddr {
    WebRTCSockaddr(Reader& r);
    constexpr WebRTCSockaddr(IPv4 ipv4)
        : ipv4(ipv4)
    {
    }
    constexpr WebRTCSockaddr(std::string_view);
    static WebRTCSockaddr from_sql_id(int64_t id)
    {
        return WebRTCSockaddr(
            IPv4(uint64_t(id & 0x0000FFFFFFFF0000) >> 16),
            uint16_t(0x000000000000FFFF & id));
    };
    int64_t to_sql_id()
    {
        return (int64_t(ipv4.data) << 16) + (int64_t(port));
    };
    auto operator<=>(const WebRTCSockaddr&) const = default;
    static constexpr std::optional<WebRTCSockaddr> parse(const std::string_view&);
#ifndef DISABLE_LIBUV
    operator sockaddr() const;
    sockaddr sock_addr() const;
#endif

    IPv4 ipv4;
};
