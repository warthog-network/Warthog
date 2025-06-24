#include "ipv6.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"

std::string IPv6::BanHandle48::to_string() const
{
    std::array<uint8_t, 16> tmp;
    tmp.fill(0);
    std::copy(data.begin(), data.end(), tmp.begin());
    IPv6 ip(tmp);
    auto s { ip.to_string() };
    s.resize(15);
    s += ":/48";
    return s;
}
std::string IPv6::BanHandle32::to_string() const
{
    std::array<uint8_t, 16> tmp;
    tmp.fill(0);
    std::copy(data.begin(), data.end(), tmp.begin());
    IPv6 ip(tmp);
    auto s { ip.to_string() };
    s.resize(10);
    s += ":/32";
    return s;
}

auto IPv6::block48_view() const -> Block48View
{
    return { std::span<const uint8_t, 6> { data.begin(), data.begin() + 6 } };
}

auto IPv6::block32_view() const -> Block32View
{
    return { std::span<const uint8_t, 4> { data.begin(), data.begin() + 4 } };
}

template <std::size_t N>
requires(N <= 16)
bool IPv6::has_prefix(const uint8_t (&cmp)[N]) const
{
    for (size_t i = 0; i < N; ++i) {
        if (cmp[i] != data[i])
            return false;
    }
    return true;
}
IPv6::IPv6(Reader& r)
    : data(r.view<16>())
{
}


std::string IPv6::to_string() const
{
    std::string out;
    out.resize(8 * 4 + 7);
    for (size_t i = 0; i < 8; ++i) {
        constexpr char map[] = "0123456789abcdef";
        size_t offset = 5 * i;
        uint8_t c = data[2 * i];
        out[offset] = map[c >> 4];
        out[offset + 1] = map[c & 15];
        c = data[2 * i + 1];
        out[offset + 2] = map[c >> 4];
        out[offset + 3] = map[c & 15];
        if (i < 7)
            out[offset + 4] = ':';
    }
    return out;
}
bool IPv6::is_rfc3849() const
{
    return has_prefix({ 0x20, 0x01, 0x0D, 0xB8 });
}

bool IPv6::is_valid() const
{
    // unspecified IPv6 address (::/128)
    return !has_prefix({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 })
        && !is_rfc3849(); // documentation address (2001:0DB8::/32)
}

bool IPv6::is_rfc3964() const
{
    return has_prefix({ 0x20, 0x02 });
}

bool IPv6::is_rfc4193() const
{
    return (at<0>() & 0xFE) == 0xFC;
}
bool IPv6::is_rfc4380() const
{
    return has_prefix({ 0x20, 0x01, 0x00, 0x00 });
}

bool IPv6::is_rfc4843() const
{
    return has_prefix({ 0x20, 0x01, 0x00 }) && (at<3>() & 0xF0) == 0x10;
}

bool IPv6::is_rfc7343() const
{
    return has_prefix({ 0x20, 0x01, 0x00 }) && (at<3>() & 0xF0) == 0x20;
}
bool IPv6::is_rfc4862() const
{
    return has_prefix({ 0xFE, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
}
bool IPv6::is_rfc6052() const
{
    return has_prefix({ 0x00, 0x64, 0xFF, 0x9B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
}
bool IPv6::is_rfc6145() const
{
    return has_prefix({ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00 });
}

bool IPv6::is_routable() const
{
    return is_valid() && !(is_rfc4862() || is_rfc4193() || is_rfc4843() || is_rfc7343() || is_loopback());
}
