#pragma once
#include "general/byte_order.hpp"
#include "ip_type.hpp"
#include <array>
#include <compare>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
class Reader;
class Writer;

class IPv6 {
public:
    class BanHandle48 {
    public:
        auto operator<=>(const BanHandle48&) const = default;
        BanHandle48(const uint8_t* p, size_t n)
            : BanHandle48(std::span<const uint8_t, 6> { p, n })
        {
        }
        BanHandle48(std::span<const uint8_t, 6> s)
        {
            std::copy(s.begin(), s.end(), data.begin());
        }
        std::string to_string() const;

    private:
        std::array<uint8_t, 6> data;
    };

    class BanHandle32 {
    public:
        auto operator<=>(const BanHandle32&) const = default;

        BanHandle32(const uint8_t* p, size_t n)
            : BanHandle32(std::span<const uint8_t, 4> { p, n })
        {
        }
        BanHandle32(std::span<const uint8_t, 4> s)
        {
            std::copy(s.begin(), s.end(), data.begin());
        }
        std::string to_string() const;

    private:
        std::array<uint8_t, 4> data;
    };
    struct Block32View : public std::span<const uint8_t, 4> { // view class for representing /32 block
        Block32View(std::span<const uint8_t, 4> s)
            : std::span<const uint8_t, 4>(std::move(s))
        {
        }
    };
    struct Block48View : public std::span<const uint8_t, 6> { // view class for representing /48 block
        Block48View(std::span<const uint8_t, 6> s)
            : std::span<const uint8_t, 6>(std::move(s))
        {
        }
    };

    constexpr static auto type() { return IpType::v6; }
    IPv6(Reader& r);
    Block48View block48_view() const;
    Block32View block32_view() const;
    static constexpr IPv6 from_data(const uint8_t* pos, size_t len = byte_size())
    {
        return { std::span<const uint8_t, 16> { pos, len } };
    }
    constexpr IPv6(std::span<const uint8_t, 16> s)
    {
        std::copy(s.begin(), s.end(), data.begin());
    }
    constexpr IPv6(std::array<uint8_t, 16> data)
        : data(data)
    {
    }
    friend Writer& operator<<(Writer&, const IPv6&);
    auto operator<=>(const IPv6& rhs) const = default;
    static constexpr std::optional<IPv6> parse(const std::string_view&);
    static constexpr size_t byte_size() { return 16; }
    bool is_loopback() const
    {
        return data == decltype(data) { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    }
    template <std::size_t N>
    requires(N <= 16)
    [[nodiscard]] bool has_prefix(const uint8_t (&)[N]) const;
    template <size_t i>
    requires(i < 16)
    uint8_t at() const
    {
        return data[i];
    }
    BanHandle32 ban_handle32() const
    {
        return { std::span<const uint8_t, 4>(data.begin(), data.begin() + 4) };
    }
    BanHandle48 ban_handle48() const
    {
        return { std::span<const uint8_t, 6>(data.begin(), data.begin() + 6) };
    }
    bool is_valid() const;
    bool is_rfc3849() const; // IPv6 documentation address (2001:0DB8::/32)
    bool is_rfc3964() const; // IPv6 6to4 tunnelling (2002::/16)
    bool is_rfc4193() const; // IPv6 unique local (FC00::/7)
    bool is_rfc4380() const; // IPv6 Teredo tunnelling (2001::/32)
    bool is_rfc4843() const; // IPv6 ORCHID (deprecated) (2001:10::/28)
    bool is_rfc7343() const; // IPv6 ORCHIDv2 (2001:20::/28)
    bool is_rfc4862() const; // IPv6 autoconfig (FE80::/64)
    bool is_rfc6052() const; // IPv6 well-known prefix for IPv4-embedded address (64:FF9B::/96)
    bool is_rfc6145() const; // IPv6 IPv4-translated address (::FFFF:0:0:0/96) (actually defined in RFC2765)
    bool is_routable() const;

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
