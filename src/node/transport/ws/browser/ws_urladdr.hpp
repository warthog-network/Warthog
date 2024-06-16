#pragma once
#include "transport/helpers/ip.hpp"
#include <optional>
#include <string>
#include <string_view>
class WSUrladdr {
    constexpr WSUrladdr(std::string_view url, std::string scheme, uint16_t port)
        : url(url)
        , scheme(std::move(scheme))
        , _port(port)
    {
    }

public:
    auto operator<=>(const WSUrladdr&) const = default;
    std::string to_string() const { return url; }
    std::string_view type_str() const
    {
        return scheme;
    }
    std::optional<IP> ip() const { return {}; }
    uint16_t port() const { return _port; };
    static std::optional<WSUrladdr> parse(const std::string& sv);
    std::string url;
    std::string scheme;
    uint16_t _port;
};
