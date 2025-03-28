#include "funds.hpp"
#include "general/errors.hpp"
#include "general/params.hpp"
#include "general/reader.hpp"
#include <cassert>
#include <charconv>
#include <cstring>
#include <limits>


Funds_uint64 Funds_uint64::parse_throw(std::string_view s, DecimalDigits d)
{
    if (auto o { Funds_uint64::parse(s, d) }; o.has_value()) {
        return *o;
    }
    throw Error(EINV_FUNDS);
}

namespace {
[[nodiscard]] std::string funds_to_string(uint64_t val, DecimalDigits decimals)
{
    const size_t d { decimals() };
    if (val == 0)
        return "0";
    static_assert(COINUNIT == 100000000);
    std::string s { std::to_string(val) };
    size_t p = s.size();
    if (p > d) {
        s.resize(p + 1);
        for (size_t i = 0; i < d; ++i)
            s[p - i] = s[p - i - 1];
        s[p - d] = '.';
        return s;
    } else {
        size_t z = d - p;
        std::string tmp;
        tmp.resize(2 + d);
        tmp[0] = '0';
        tmp[1] = '.';
        for (size_t i = 0; i < z; ++i)
            tmp[2 + i] = '0';
        memcpy(&tmp[2 + z], s.data(), p);
        return tmp;
    }
}
}
std::string Funds_uint64::to_string(DecimalDigits decimals) const
{
    return funds_to_string(val, decimals);
}
Funds_uint64::Funds_uint64(Reader& r)
    : FundsBase<Funds_uint64>(from_value_throw(r))
{
}

std::optional<Funds_uint64> Funds_uint64::parse(std::string_view s, DecimalDigits digits)
{
    const size_t d { digits() };
    static_assert(DecimalDigits::max <= 18); // otherwise we need to add entries to this buf
    char buf[20]; // max uint64_t has 20 digits max
    size_t dotindex = 0;
    bool dotfound = false;
    size_t i;
    auto out { buf };
    for (i = 0; i < s.length(); ++i) {
        if (i > 20 || (i == 19 && dotfound == false))
            return {}; // too many digits
        char c = s[i];
        if (c == '.') {
            if (!dotfound) {
                dotfound = true;
                dotindex = i;
            } else
                return {};
        } else if (c >= '0' && c <= '9') {
            *(out++) = c;
        } else {
            return {};
        }
    }

    uint64_t v;
    auto [ptr, ec] { std::from_chars(buf, out, v) };
    if (ec != std::errc() || ptr != out)
        return {};

    size_t afterDotDigits = dotfound ? i - dotindex - 1 : 0;
    if (afterDotDigits > d)
        return {};
    const size_t zeros = d - afterDotDigits;
    for (size_t i { 0 }; i < zeros; ++i) {
        if (std::numeric_limits<uint64_t>::max() / 10 < v)
            return {};
        v *= 10;
    }
    return Funds_uint64::from_value(v);
}
std::optional<Wart> Wart::parse(std::string_view s)
{
    auto p { Funds_uint64::parse(s, DecimalDigits::digits8()) };
    if (p)
        return Wart::from_value(p->value());
    return {};
}
Wart Wart::parse_throw(std::string_view s)
{
    if (auto o { parse(s) }; o.has_value()) {
        return *o;
    }
    throw Error(EINV_FUNDS);
}

std::string Wart::to_string() const
{
    return funds_to_string(val, DecimalDigits::digits8());
}
Wart::Wart(Reader& r)
    : FundsBase<Wart>(from_value_throw(r))
{
}
