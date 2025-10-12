#pragma once
#include "general/funds.hpp"
#include "price.hpp"

namespace defi {

struct Order_uint64 {
    Order_uint64(Reader& r)
        : amount(r)
        , limit(r) { };
    Order_uint64(Funds_uint64 amount, Price_uint64 limit)
        : amount(std::move(amount))
        , limit(std::move(limit)) { };
    Funds_uint64 amount;
    Price_uint64 limit;
};

struct BaseQuote_uint64;
struct Delta_uint64 {
    bool operator==(const Delta_uint64&) const = default;
    bool isQuote { false };
    Funds_uint64 amount;
    BaseQuote_uint64 base_quote() const;
};
struct NonzeroDelta_uint64 {
    explicit NonzeroDelta_uint64(bool isQuote, NonzeroFunds_uint64 amount)
        : isQuote_(std::move(isQuote))
        , amount_(std::move(amount))
    {
    }
    Delta_uint64 get() const { return { isQuote_, amount_ }; }
    bool is_quote() const { return isQuote_; }
    auto amount() const { return amount_; }
    bool operator==(const NonzeroDelta_uint64&) const = default;

private:
    bool isQuote_ { false };
    NonzeroFunds_uint64 amount_;
};

struct BaseQuote_uint64 {
    Funds_uint64 base;
    Funds_uint64 quote;
    bool operator==(const BaseQuote_uint64&) const = default;
    BaseQuote_uint64(Funds_uint64 base, Funds_uint64 quote)
        : base(base)
        , quote(quote)
    {
    }
    BaseQuote_uint64(Reader& r)
        : base(r)
        , quote(r) { };
    // BaseQuote_uint64 operator-(const Delta_uint64& bq) const
    // {
    //     auto res { *this };
    //     if (bq.isQuote)
    //         res.quote.subtract_assert(bq.amount);
    //     else
    //         res.base -= bq.amount;
    //     return res;
    // }
    Delta_uint64 excess(Price_uint64 p) const // computes excess
    {
        auto q { multiply_floor(base.value(), p) };
        if (q.has_value() && *q <= quote) // too much base
            return { true, diff_assert(quote, Funds_uint64::from_value_throw(*q)) };
        auto b { divide_floor(quote.value(), p) };
        assert(b.has_value()); // TODO: verify assert by checking precision of
                               // divide_floor
        assert(*b <= base);
        return { false, diff_assert(base, Funds_uint64::from_value_throw(*b)) };
    }
    auto price() const { return PriceRelative_uint64::from_fraction(quote.value(), base.value()); }
};

inline BaseQuote_uint64 Delta_uint64::base_quote() const
{
    if (isQuote)
        return { 0, amount };
    return { amount, 0 };
}
}
