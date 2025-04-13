#include "funds.hpp"
#include "general/errors.hpp"
#include "general/params.hpp"
#include "general/reader.hpp"
#include <cassert>
#include <charconv>
#include <cstring>
#include <limits>

std::optional<FundsDecimal> FundsDecimal::parse(std::string_view s)
{
    std::optional<FundsDecimal> res;
    constexpr const size_t N { 20 }; // max uint64_t has 20 digits max
    char buf[N];
    size_t i { 0 };
    uint8_t digits { 0 };
    bool dotfound { false };
    for (auto c : s) {
        if (c >= '0' && c <= '9') {
            if (i >= N)
                return res;
            buf[i++] = c;
            if (dotfound)
                digits += 1;
        } else if (c == '.') {
            if (dotfound)
                return res;
            dotfound = true;
        } else {
            return res;
        }
    }
    uint64_t v;
    auto [ptr, ec] { std::from_chars(buf, buf + i, v) };
    if (ec != std::errc() || ptr != buf + i)
        return {};
    return FundsDecimal { v, digits };
}
FundsDecimal::FundsDecimal(std::string_view s)
    : FundsDecimal([&s]() {
        if (auto p { parse(s) })
            return *p;
        throw Error(EINV_FUNDS);
    }())
{
}

Funds_uint64 Funds_uint64::parse_throw(std::string_view s, TokenPrecision d)
{
    if (auto o { Funds_uint64::parse(s, d) }; o.has_value()) {
        return *o;
    }
    throw Error(EINV_FUNDS);
}

[[nodiscard]] std::string FundsDecimal::to_string() const
{
    const size_t d { precision };
    if (v == 0)
        return "0";
    static_assert(COINUNIT == 100000000);
    std::string s { std::to_string(v) };
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

std::string Funds_uint64::to_string(TokenPrecision decimals) const
{
    return to_decimal(decimals).to_string();
}

Funds_uint64::Funds_uint64(Reader& r)
    : FundsBase<Funds_uint64>(from_value_throw(r))
{
}

std::optional<Funds_uint64> Funds_uint64::parse(std::string_view s, TokenPrecision digits)
{
    auto fd { FundsDecimal::parse(s) };
    if (!fd)
        return {};
    return parse(*fd, digits);
}

std::optional<Funds_uint64> Funds_uint64::parse(FundsDecimal fd, TokenPrecision digits)
{
    if (fd.precision > digits())
        return {};
    size_t zeros { size_t(digits()) - size_t(fd.precision) };
    auto v { fd.v };

    for (size_t i { 0 }; i < zeros; ++i) {
        if (std::numeric_limits<uint64_t>::max() / 10 < v)
            return {};
        v *= 10;
    }
    return Funds_uint64::from_value(v);
}

std::optional<Wart> Wart::parse(std::string_view s)
{
    auto fd { FundsDecimal::parse(s) };
    if (!fd)
        return {};
    return parse(*fd);
}

std::optional<Wart> Wart::parse(FundsDecimal fd)
{
    auto p { Funds_uint64::parse(fd, TokenPrecision::digits8()) };
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
    return funds_to_string(val, TokenPrecision::digits8());
}
Wart::Wart(Reader& r)
    : FundsBase<Wart>(from_value_throw(r))
{
}
