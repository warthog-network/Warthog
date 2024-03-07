#pragma once

#include "general/byte_order.hpp"
#include <compare>
#include <cstdint>
#include <optional>
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
    uint32_t data;
    std::string to_string() const;
};

constexpr std::optional<IPv4> IPv4::parse(const std::string_view& s)
{
    uint32_t tmp{0};

    int saw_digit = 0;
    int index = 0;

    uint32_t nw = 0;
    for (char c : s) {
        if (c >= '0' && c <= '9') {
            nw = nw * 10 + (c - '0');
            if ((saw_digit && nw == 0) || nw > 255)
                return {};
            tmp+= uint32_t(nw) << 8*(3-index);
            saw_digit = 1;
        } else if (c == '.' && saw_digit) {
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
    return IPv4(ntoh32(tmp));
}
