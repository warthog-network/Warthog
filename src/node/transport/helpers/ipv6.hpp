#pragma once

#include "general/byte_order.hpp"
#include <compare>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <array>
class Reader;
#ifndef DISABLE_LIBUV
struct sockaddr_in;
#endif

struct IPv6 {
    IPv6(Reader& r);
#ifndef DISABLE_LIBUV
    IPv6(const sockaddr_in& sin);
#endif
    constexpr IPv6(std::array<uint8_t,16> data)
        : data(data)
    {
    }
    auto operator<=>(const IPv6& rhs) const = default;
    static constexpr std::optional<IPv6> parse(const std::string_view&);
    constexpr IPv6(const std::string_view& s)
        : IPv6(
            [&] {
                auto ea { parse(s) };
                if (ea)
                    return *ea;
                throw std::runtime_error("Cannot parse ip address \"" + std::string(s) + "\".");
            }()) {};
    std::array<uint8_t,16> data;
    std::string to_string() const;
};

constexpr std::optional<IPv6> IPv6::parse(const std::string_view& s)
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
    return IPv6(tmp);
}
