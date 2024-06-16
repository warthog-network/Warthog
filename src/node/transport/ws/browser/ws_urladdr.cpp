#include "ws_urladdr.hpp"
#include <charconv>
#include <regex>

namespace {
std::optional<uint16_t> parse_port(std::string_view portStr)
{
    uint16_t res;
    auto result = std::from_chars(portStr.begin(), portStr.end(), res);
    if (result.ec != std::errc {} || result.ptr != portStr.end())
        return {};
    return res;
}
}

std::optional<WSUrladdr> WSUrladdr::parse(const std::string& url)
{
    const static std::regex re(
        R"((?:(wss?):)?(?://(?:\[([\d:]+)\]|([^:/?#]+))(?::(\d+))?)?([^?#]*(?:\?[^#]*)?)(?:#.*)?)");

    std::smatch m;
    if (!std::regex_match(url.begin(), url.end(), m, re)) {
        return {};
    }

    auto scheme { m[1].str() };
    auto port_str { m[4].str() };
    if (auto p { parse_port(port_str) }; p.has_value()) {
        return WSUrladdr { url, scheme, *p };
    }
    return {};
}
