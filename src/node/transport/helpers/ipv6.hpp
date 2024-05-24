#pragma once
#include "general/byte_order.hpp"
#include "ip_type.hpp"
#include <array>
#include <compare>
#include <optional>
#include <stdexcept>
#include <string>
class Reader;
class Writer;

class IPv6 {
public:
    constexpr static auto type() { return IpType::v6; }
    IPv6(Reader& r);
    constexpr IPv6(std::array<uint8_t, 16> data)
        : data(data)
    {
    }
    friend Writer& operator<<(Writer&, const IPv6&);
    auto operator<=>(const IPv6& rhs) const = default;
    static constexpr std::optional<IPv6> parse(const std::string_view&);
    static consteval size_t byte_size() { return 16; }
    bool is_localhost() const
    {
        return data == decltype(data) { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    }

    constexpr IPv6(const std::string_view& s)
        : IPv6(
            [&] {
                auto ea { parse(s) };
                if (ea)
                    return *ea;
                throw std::runtime_error("Cannot parse ip address \"" + std::string(s) + "\".");
            }()) {};
    std::array<uint8_t, 16> data;
    std::string to_string() const;
};

constexpr std::optional<IPv6> IPv6::parse(const std::string_view& s)
{
    auto write_part = [](uint8_t* dst, std::string_view part) -> bool {
        auto hex_digit = [](bool& good, char c) -> uint8_t {
            switch (c) {
            case '0':
                return 0;
            case '1':
                return 1;
            case '2':
                return 2;
            case '3':
                return 3;
            case '4':
                return 4;
            case '5':
                return 5;
            case '6':
                return 6;
            case '7':
                return 7;
            case '8':
                return 8;
            case '9':
                return 9;
            case 'a':
            case 'A':
                return 10;
            case 'b':
            case 'B':
                return 11;
            case 'c':
            case 'C':
                return 12;
            case 'd':
            case 'D':
                return 13;
            case 'e':
            case 'E':
                return 14;
            case 'f':
            case 'F':
                return 15;
            default:
                good = false;
            }
            return 0;
        };
        bool good = true;
        const auto s { part.size() };
        if (s == 0 || s > 4)
            return false;
        uint32_t v { hex_digit(good, part[0]) };
        for (size_t i = 1; i < s; ++i) {
            v <<= 4;
            v |= hex_digit(good, part[i]);
        }
        dst[0] = v >> 8;
        dst[1] = v & 0xFF;
        return good;
    };

    auto count_colons = [](std::string_view s) -> size_t {
        size_t n = 0;
        for (auto c : s) {
            if (c == ':')
                n += 1;
        }
        return n;
    };

    const size_t l
        = s.length();
    if (l < 2 || l > 200)
        return {};
    std::array<uint8_t, 16> out;
    constexpr auto npos = std::string::npos;
    bool doubleColon = false;
    size_t i = 0;
    size_t start = 0;
    while (true) {
        size_t pos = s.find(":", start);
        if (pos == npos) {
            if (i != 7 || !write_part(&out[2 * 7], s.substr(start)))
                return {};
            return out;
        } else {
            if (pos == start) {
                if (doubleColon)
                    return {};
                if (pos == 0) {
                    if (s[1] != ':')
                        return {};
                    pos = start = 1;
                }
                doubleColon = true;
                size_t n = count_colons(s.substr(pos + 1));
                if (n + i + 1 >= 7)
                    return {};
                size_t i2 = 7 - n;
                while (i < i2) {
                    out[2 * i] = 0;
                    out[2 * i + 1] = 0;
                    i += 1;
                }
                if (pos + 1 == s.length()) {
                    out[2 * i] = 0;
                    out[2 * i + 1] = 0;
                    return out;
                }
            } else {
                if (!write_part(&out[2 * i], s.substr(start, pos - start)))
                    return {};
                i += 1;
            }
        }
        start = pos + 1;
    }
}
