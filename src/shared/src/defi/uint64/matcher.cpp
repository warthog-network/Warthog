#include "matcher.hpp"
#include <numeric>
#include <variant>

namespace defi {
wrt::optional<NonzeroDelta_uint64> FilledAndPool::balance_pool_interaction() const
{
    struct ret_t {
        std::strong_ordering rel;
        using Pool128Ratio = Ratio128;
        struct Fill64Ratio {
            uint64_t a, b;
        };
        bool exceeded() const { return std::holds_alternative<Ratio128>(v); }
        auto& get_unexceeded_ratio() const { return std::get<Fill64Ratio>(v); };
        auto& get_exceeded_ratio() const { return std::get<Ratio128>(v); };
        std::variant<Ratio128, Fill64Ratio> v;
    };

    auto nondecreasing_relation_base = [&](uint64_t baseToPool) -> ret_t {
        // pool price after we swap `baseToPool` from base to quote (sell) at the pool
        auto poolRatio { pool.price_ratio_added_base(baseToPool) };

        // if we subtract `baseToPool` from `in.base`,
        // new in price is in_numerator/in_denominator
        auto in_numerator { in.quote.value() };
        auto in_denominator { in.base.value() - baseToPool };

        // to compare the prices we compare these products
        auto pool_price_score { poolRatio.numerator * in_denominator };
        auto in_price_score { poolRatio.denominator * in_numerator };

        // as we push more base from in to pool (i.e. sell), the pool price will at some point
        // be smaller than the in price, to make the relation nondecreasing (less -> equal -> greater)
        // in the argument `baseToPool` we compare in_price_score <=> pool_price_score.
        auto rel { in_price_score <=> pool_price_score };

        // the second return argument shall balance the two converged options of different relation,
        // we will pick the one that maximizes the min price
        // -> save the min price in second argument.
        // We swap denominator and numerator to avoid case distinction for comparison
        // ("maximize min price" for rel_base_asc, "minimize max price" for rel_quote_asc)
        // on callser side
        if (rel == std::strong_ordering::greater) // in price greater
            return { rel, ret_t::Pool128Ratio { poolRatio.denominator, poolRatio.numerator } };
        else
            return { rel, ret_t::Fill64Ratio { in_denominator, in_numerator } };
    };

    auto nondecreasing_releation_quote = [&](uint64_t quoteToPool) -> ret_t {
        // pool price after we swap `quoteToPool` from quote to base (buy) at the pool
        auto poolRatio { pool.price_ratio_added_quote(quoteToPool) };

        // if we subtract `quoteToPool` from `in.quote`,
        // new in price is in_numerator/in_denominator
        auto in_numerator { in.quote.value() - quoteToPool };
        auto in_denominator { in.base.value() };

        // to compare the prices we compare these products
        auto pool_price_score { poolRatio.numerator * in_denominator };
        auto in_price_score { poolRatio.denominator * in_numerator };

        // as we push more quote from in to pool (i.e. buy), the pool price will at some point
        // be greater than the in price, to make the relation nondecreasing (less -> equal -> greater)
        // in the argument `quoteToPool` we compare pool_price_score <=> in_price_score.
        auto rel { pool_price_score <=> in_price_score };

        // the second return argument shall balance the two converged options of different relation,
        // we will pick the one that minimizes the max price -> save the max price in second argument
        if (rel == std::strong_ordering::greater)
            return { rel, ret_t::Pool128Ratio { poolRatio } };
        else
            return { rel, ret_t::Fill64Ratio { in_numerator, in_denominator } };
    };

    auto bisect = [](ret_t::Fill64Ratio ratio0, uint64_t v1,
                      auto asc_fun) {
        if (v1 == 0)
            return v1;
        ret_t r { asc_fun(v1) };
        assert(r.rel != std::strong_ordering::less);
        if (r.rel == std::strong_ordering::equal)
            return v1;
        auto ratio1 { r.get_exceeded_ratio() };
        uint64_t v0 { 0 };
        while (true) {
            uint64_t v { std::midpoint(v0, v1) };
            if (v0 == v)
                break;
            ret_t ret = asc_fun(v);
            if (ret.exceeded()) {
                v1 = v;
                ratio1 = ret.get_exceeded_ratio();
            } else {
                v0 = v;
                ratio0 = ret.get_unexceeded_ratio();
            }
        }
        if (ratio1.denominator * ratio0.a < ratio1.numerator * ratio0.b)
            return v0;
        return v1;
    };
    auto baseRet { nondecreasing_relation_base(0) };
    auto quoteRet { nondecreasing_releation_quote(0) };
    auto make_toPool = [&](bool isQuote,
                           uint64_t toPool) -> wrt::optional<NonzeroDelta_uint64> {
        if (toPool == 0)
            return {};
        return NonzeroDelta_uint64 { isQuote, NonzeroFunds_uint64(toPool) };
    };
    if (baseRet.rel == std::strong_ordering::greater) {
        // need to push quote to pool
        assert(quoteRet.rel == std::strong_ordering::less);
        uint64_t toPoolAmount {
            bisect(quoteRet.get_unexceeded_ratio(), in.quote.value(),
                [&](uint64_t toPool) { return nondecreasing_releation_quote(toPool); })
        };
        return make_toPool(true, toPoolAmount);
    } else {
        assert(quoteRet.rel != std::strong_ordering::less);
        // need to push base to pool
        auto toPoolAmount {
            bisect(baseRet.get_unexceeded_ratio(), in.base.value(),
                [&](uint64_t toPool) { return nondecreasing_relation_base(toPool); })
        };
        return make_toPool(false, toPoolAmount);
    }
}
}
