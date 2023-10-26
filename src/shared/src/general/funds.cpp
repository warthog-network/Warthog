#include "funds.hpp"
#include "general/errors.hpp"
#include "general/params.hpp"
#include "general/writer.hpp"
#include <cassert>
#include <cstring>
Funds Funds::throw_parse(std::string_view s){
    if (auto o{Funds::parse(s)}; o.has_value()) {
        return *o;
    }
    throw Error(EMALFORMED);
}

void Funds::assert_bounds() const
{
    assert(val <= MAXSUPPLY);
};
std::string Funds::format() const
{
    if (val == 0)
        return "0 WRT";
    static_assert(COINUNIT == 100000000);
    std::string s { std::to_string(val) };
    size_t p = s.size();
    if (p >= 9) {
        s.resize(p + 1 + 4);
        for (size_t i = 0; i < 8; ++i)
            s[p - i] = s[p - i - 1];
        s[p - 8] = '.';
        std::memcpy(&(s[p + 1]), " WRT", 4);
    } else {
        if (p < 3) {
            s.resize(p + 5);
            std::memcpy(&(s[p]), " nWRT", 5);
        } else if (p < 6) {
            s.resize(p + 1 + 5);
            for (size_t i = 0; i < 2; ++i)
                s[p - i] = s[p - i - 1];
            s[p - 2] = '.';
            std::memcpy(&(s[p + 1]), " μWRT", 5);
        } else {
            s.resize(p + 1 + 5);
            for (size_t i = 0; i < 5; ++i)
                s[p - i] = s[p - i - 1];
            s[p - 5] = '.';
            std::memcpy(&(s[p + 1]), " mWRT", 5);
        }
    }
    return s;
}

std::string Funds::to_string() const
{
    if (val == 0)
        return "0";
    static_assert(COINUNIT == 100000000);
    std::string s { std::to_string(val) };
    size_t p = s.size();
    if (p >= 9) {
        s.resize(p + 1);
        for (size_t i = 0; i < 8; ++i)
            s[p - i] = s[p - i - 1];
        s[p - 8] = '.';
        return s;
    } else {
        size_t z = 8 - p;
        std::string tmp;
        tmp.resize(10);
        tmp[0] = '0';
        tmp[1] = '.';
        for (size_t i = 0; i < z; ++i)
            tmp[2 + i] = '0';
        memcpy(&tmp[2 + z], s.data(), p);
        return tmp;
    }
}


std::optional<Funds> Funds::parse(std::string_view s)
{
    char buf[17]; // 16 digits max for WRT balances
    size_t dotindex = 0;
    bool dotfound = false;
    size_t i;
    for (i = 0; i<s.length(); ++i) {
        if (i >= 18 || (i == 17 && dotfound == false))
            return {}; // too many digits
        char c = s[i];
        if (c == '.') {
            if (!dotfound) {
                dotfound = true;
                dotindex = i;
            } else
                return {};
        } else if (c >= '0' && c <= '9') {
            if (dotfound)
                buf[i - 1] = c;
            else
                buf[i] = c;
        } else {
            return {};
        }
    }
    uint64_t powers10[9] = {
        1, 10, 100, 1000, 10000,
        100000, 1000000, 10000000, 100000000
    };

    if (dotfound)
        buf[i - 1] = '\0';
    else
        buf[i] = '\0';
    if (dotfound) {
        if (dotindex > 8)
            return {};
        size_t afterDotDigits = (i - dotindex - 1);
        if (afterDotDigits > 8)
            return {};
        size_t addzeros = 8 - afterDotDigits;
        return Funds(uint64_t(std::stoull(buf)) * powers10[addzeros]); // static_assert coinunit==10000000
    } else {
        if (i > 8)
            return {};
        return Funds { uint64_t(std::stoull(buf) * powers10[8]) }; // static_assert coinunit==10000000
    }
    return {};
}
