#pragma once

#include "general/byte_order.hpp"
#include <compare>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
class Reader;
struct sockaddr_in;

struct IPv4 {
    IPv4(Reader& r);
    IPv4(const sockaddr_in& sin);
    constexpr IPv4(uint32_t data = 0)
        : data(data)
    {
    }
    bool is_valid(bool allowLocalhost = true) const;
    bool is_localhost() const;
    auto operator<=>(const IPv4& rhs) const = default;
    static constexpr std::optional<IPv4> parse(const std::string_view&);
    constexpr IPv4(const std::string_view& s)
        : IPv4(
            [&] {
                auto ea { parse(s) };
                if (ea)
                    return *ea;
                throw std::runtime_error("Cannot parse ip address \"" + std::string(s) + "\".");
            }()) {};
    uint32_t data;
    std::string to_string() const;
};

constexpr std::optional<IPv4> IPv4::parse(const std::string_view& s)
{
    uint32_t tmp { 0 };
    int saw_digit = 0;
    int index = 0;

    uint32_t nw = 0;
    for (char c : s) {
        if (c >= '0' && c <= '9') {
            nw = nw * 10 + (c - '0');
            if ((saw_digit && nw == 0) || nw > 255)
                return {};
            saw_digit = 1;
        } else if (c == '.' && saw_digit) {
            tmp += uint32_t(nw) << 8 * (3 - index);
            if (++index >= 4)
                return {};
            saw_digit = 0;
            nw = 0;
        } else {
            return {};
        }
    }
    if (index != 3 || !saw_digit)
        return {};
    tmp += uint32_t(nw) << 8 * (3 - index);
    return IPv4(tmp);
}
