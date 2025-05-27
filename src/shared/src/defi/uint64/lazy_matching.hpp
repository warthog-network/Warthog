#pragma once

#include "matcher.hpp"
#include "orderbook.hpp"
#include "pool.hpp"
using namespace std;

namespace defi {

[[nodiscard]] inline MatchResult_uint64 match_lazy(auto& loaderSellAsc, auto& loaderBuyDesc, const PoolLiquidity_uint64& pool)
{
    assert(pool.price()); // TODO: ensure the pool is non-degenerate
    const auto price { *pool.price() };

    Orderbook_uint64 ob;
    std::optional<Price_uint64> lower, upper;
    BaseQuote_uint64 filled { 0, 0 };

    // load sell orders below pool price
    size_t I { 0 }; // sell index bound
    std::optional<Price_uint64> p;
    while (true) {
        auto np { loaderSellAsc.next_price() };
        if (!np)
            break;
        assert(!p || *p < *np); // prices must be strictly increasing
        p = np;
        if (*p <= price) {
            Order_uint64 o { loaderSellAsc.load_next_order() };
            ob.insert_largest_base(o);
            lower = o.limit;
            filled.base.add_assert(o.amount.value());
            I = ob.base_asc_sell().size();
        } else {
            upper = *p;
            break;
        }
    }

    // load buy orders above pool price
    size_t J { 0 }; // buy index bound
    p.reset();
    while (true) {
        auto np { loaderBuyDesc.next_price() };
        if (!np)
            break;
        assert(!p || *p > *np); // prices must be strictly decreasing
        p = np;
        if (*p > price) { // now require strictness to avoid selecting degenerate (zero-length) section
            Order_uint64 o { loaderBuyDesc.load_next_order() };
            ob.insert_smallest_quote(o);
            if (!upper || *upper > o.limit)
                upper = o.limit;
            filled.quote.add_assert(o.amount.value());
            J = ob.quote_desc_buy().size();
        } else {
            if (!lower || *lower < *p)
                lower = *p;
            break;
        }
    }

    assert(!upper || upper != lower); // we cannot have degenerate (zero-length) section

    auto more_quote_less_base = [&](Price_uint64 p) {
        Delta_uint64 toPool { filled.excess(p) };
        return !pool.modified_pool_price_exceeds(toPool, p);
    };

    auto upper_buy_bound { [&]() -> const Order_uint64* {
        if (J == 0)
            return nullptr;
        return &ob.quote_desc_buy()[(J - 1)];
    } };

    auto lower_sell_bound { [&]() -> const Order_uint64* {
        if (I == 0)
            return nullptr;
        return &ob.base_asc_sell()[I - 1];
    } };

    auto shift_buy_higher { [&]() {
        assert(J != 0);
        filled.quote.subtract_assert(upper_buy_bound()->amount);
        J -= 1;
    } };

    auto shift_sell_smaller { [&]() {
        assert(I != 0);
        filled.base.subtract_assert(lower_sell_bound()->amount);
        I -= 1;
    } };

    if (upper && !more_quote_less_base(*upper)) {
        std::optional<Price_uint64> np { loaderSellAsc.next_price() };
        while (np) {

            while (auto b { upper_buy_bound() }) {
                if (b->limit < *np)
                    shift_buy_higher();
                else
                    break;
            }

            if (more_quote_less_base(*np))
                break;
            Order_uint64 o { loaderSellAsc.load_next_order() };
            assert(*np == o.limit);
            ob.insert_largest_base(o);
            filled.base.add_assert(o.amount.value());
            np = loaderSellAsc.next_price();
        }
    } else if (lower && more_quote_less_base(*lower)) {
        std::optional<Price_uint64> np { loaderBuyDesc.next_price() };
        while (np) {

            while (auto b { lower_sell_bound() }) {
                if (b->limit > *np)
                    shift_sell_smaller();
                else
                    break;
            }


            if (!more_quote_less_base(*np))
                break;
            Order_uint64 o { loaderBuyDesc.load_next_order() };
            assert(*np == o.limit);
            ob.insert_smallest_quote(o);
            filled.quote.add_assert(o.amount.value());
            np = loaderBuyDesc.next_price();
        }
    }

    using namespace std;
    return ob.match(pool);
}
}
