#include "ws_urladdr.hpp"
#include "spdlog/spdlog.h"
#include <charconv>
#include <regex>

namespace {
std::optional<uint16_t> parse_port(std::string_view scheme, std::string_view portStr)
{
    if (portStr.length() == 0) {
        if (scheme.length() == 3)
            return 433;
        return 80;
    }
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
        R"((?:(wss?):)?(?:\/\/(?:\[([\d:]+)\]|([^:\/?#]+))(?::(\d+))?)?([^?#]*(?:\?[^#]*)?)(?:#.*)?)");

    std::smatch m;
    if (std::regex_match(url.begin(), url.end(), m, re)) {
        auto scheme { m[1].str() };
        auto port_str { m[4].str() };
        if (auto p { parse_port(scheme, port_str) }; p.has_value()) {
            return WSUrladdr { url, scheme, *p };
        }
    }
    spdlog::warn("Parsing failed: {} is not a websocket URL", url);
    return {};
}
