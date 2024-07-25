#pragma once
#include "transport/helpers/ip.hpp"
#include <optional>
#include <string>
#include <string_view>
class WSUrladdr {
    constexpr WSUrladdr(std::string_view url, std::string scheme, uint16_t port)
        : url(url)
        , scheme(std::move(scheme))
        , port(port)
    {
    }

public:
    auto operator==(const WSUrladdr& addr) const
    {
        return url == addr.url;
    }
    auto operator<=>(const WSUrladdr& addr) const
    {
        return url <=> addr.url;
    }

    std::string to_string() const { return url; }
    std::string_view type_str() const
    {
        return scheme;
    }
    std::optional<IP> ip() const { return {}; }
    static std::optional<WSUrladdr> parse(const std::string& sv);
    std::string url;
    std::string scheme;
    uint16_t port;
};
