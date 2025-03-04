#include "defi.hpp"
#include "uint64/lazy_matching.hpp"

namespace defi {
MatchResult BuySellOrders::match_assert_lazy(Pool& p)
{
    auto res { buySellOrders_uint64.match(p.pool_uint64) };

    auto loadBaseAsc {
        [&, i = size_t(0)]() mutable -> std::optional<Order_uint64> {
            auto& baseAscSell { buySellOrders_uint64.base_asc_sell() };
            if (i == baseAscSell.size())
                return {};
            return baseAscSell[i++];
        }
    };

    auto loadQuoteDesc {
        [&, i = size_t(0)]() mutable -> std::optional<Order_uint64> {
            auto& baseDescBuy { buySellOrders_uint64.quote_desc_buy() };
            if (i == baseDescBuy.size())
                return {};
            return baseDescBuy[i++];
        }
    };
    auto res2 { match_lazy(loadBaseAsc, loadQuoteDesc, p.pool_uint64) };
    assert(res == res2); // verify that indeed lazy matching is correct
    return res;
}
}
