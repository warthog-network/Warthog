#pragma once
#include "src/defi/price.hpp"
#include <cstdint>

namespace defi {
struct Order_uint64 {
    uint64_t amount;
    Price limit;
};

struct BaseQuote_uint64;
struct Delta_uint64 {
    bool operator==(const Delta_uint64&) const = default;
    bool isQuote { false };
    uint64_t amount;
    BaseQuote_uint64 base_quote() const;
};

struct BaseQuote_uint64 {
    uint64_t base;
    uint64_t quote;
    bool operator==(const BaseQuote_uint64&) const = default;
    BaseQuote_uint64(uint64_t base, uint64_t quote)
        : base(base)
        , quote(quote)
    {
    }
    BaseQuote_uint64 operator-(const Delta_uint64& bq) const
    {
        auto res { *this };
        if (bq.isQuote)
            res.quote -= bq.amount;
        else
            res.base -= bq.amount;
        return res;
    }
    Delta_uint64 excess(Price p) const // computes excess
    {
        auto q { multiply_floor(base, p) };
        if (q.has_value() && *q <= quote) // too much base
            return { true, quote - *q };
        auto b { divide_floor(quote, p) };
        assert(b.has_value()); // TODO: verify assert by checking precision of
                               // divide_floor
        assert(*b <= base);
        return { false, base - *b };
    }
    auto price() const { return PriceRelative::from_fraction(quote, base); }
};

inline BaseQuote_uint64 Delta_uint64::base_quote() const
{
    if (isQuote)
        return { 0, amount };
    return { amount, 0 };
}
}
