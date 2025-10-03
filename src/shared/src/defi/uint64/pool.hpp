#pragma once
#include "price.hpp"
#include "types.hpp"
#include <cstdint>

namespace defi {
struct PoolLiquidity_uint64 : public BaseQuote_uint64 {
    using BaseQuote_uint64::BaseQuote_uint64;

    Ratio128 price_ratio_added_quote(uint64_t quoteToPool) const
    {
        return {
            .numerator { Prod128(quote.value() + quoteToPool, quote.value() + quoteToPool) },
            .denominator { Prod128(quote.value(), base.value()) }
        };
    }

    Ratio128 price_ratio_added_base(uint64_t baseToPool) const
    {
        return {
            .numerator { Prod128(base.value(), quote.value()) },
            .denominator { Prod128(base.value() + baseToPool, base.value() + baseToPool) }
        };
    }

    // relation of pool price (affected by adding given quoteToPool) to given price
    [[nodiscard]] std::strong_ordering rel_quote_price(uint64_t quoteToPool,
        Price_uint64 p) const
    {
        return compare_fraction(price_ratio_added_quote(quoteToPool), p);
    }

    // relation of pool price (affected by pushing given baseToPool) to given price
    [[nodiscard]] std::strong_ordering rel_base_price(uint64_t baseToPool,
        Price_uint64 p) const
    {
        return compare_fraction(price_ratio_added_base(baseToPool), p);
    }

    [[nodiscard]] bool modified_pool_price_exceeds(const Delta_uint64& toPool, Price_uint64 p) const
    {
        if (toPool.isQuote)
            return rel_quote_price(toPool.amount.value(), p) == std::strong_ordering::greater;
        else
            return rel_base_price(toPool.amount.value(), p) != std::strong_ordering::less;
    }

    [[nodiscard]] Funds_uint64 sell(Funds_uint64 baseAdd, uint64_t feeE4 = 10)
    {
        if (baseAdd == 0)
            return 0;
        auto quoteDelta = swapped_amount(base.value(), baseAdd.value(), quote.value(), feeE4);
        quote.subtract_assert(Funds_uint64(quoteDelta));
        base.add_assert(Funds_uint64(baseAdd));
        return quoteDelta;
    }

    [[nodiscard]] Funds_uint64 buy(Funds_uint64 quoteAdd, uint64_t feeE4 = 10)
    {
        if (quoteAdd == 0)
            return 0;
        auto baseDelta = swapped_amount(quote.value(), quoteAdd.value(), base.value(), feeE4);
        base.subtract_assert(Funds_uint64::from_value_throw(baseDelta));
        quote.add_assert(quoteAdd);
        return baseDelta;
    }

private:
    static uint64_t discount(uint64_t value, uint16_t feeE4)
    {
        if (value == 0)
            return 0;
        auto shift = std::countl_zero(value);
        auto v { value << shift };
        constexpr size_t E4 { 10000 };

        auto a = E4 - feeE4;
        auto b = E4;
        return ((v / b) * a + ((v % b) * a) / b) >> shift;
    }

    static uint64_t swapped_amount(uint64_t a0, uint64_t a_add, uint64_t b0, uint64_t feeE4)
    {
        auto bnew { Prod128(a0, b0).divide_ceil(a0 + a_add) };
        assert(bnew.has_value()); // cannot overflow
        const auto b1 { *bnew };
        assert(b1 <= b0);
        uint64_t bDelta { b0 - b1 };
        return discount(bDelta, feeE4);
    }
};

class Pool_uint64 : public PoolLiquidity_uint64 {

public:
    Pool_uint64(Funds_uint64 base, Funds_uint64 quote, Funds_uint64 shares)
        : PoolLiquidity_uint64(base, quote)
        , sharesTotal(shares.value())
    {
    }
    Pool_uint64()
        : Pool_uint64(0, 0, 0)
    {
    }
    bool nonzero() const { return *this != Pool_uint64(0, 0, 0); }
    [[nodiscard]] Funds_uint64 deposit(Funds_uint64 addBase, Funds_uint64 addQuote)
    {
        auto s0 { Prod128(base, quote).sqrt() };
        base.add_assert(addBase);
        quote.add_assert(addQuote);
        auto s1 { Prod128(base, quote).sqrt() };
        assert(s1 >= s0);
        if (sharesTotal == 0) {
            sharesTotal = s1;
            return s1;
        } else {
            Nonzero_uint64 nonzero_s0 { s0 }; // nonzero because 0 < sharesTotal <= s0
            auto newSharesTotal { Prod128(sharesTotal, s1).divide_floor(nonzero_s0) };
            assert(newSharesTotal.has_value()); // no overflow because sharesTotal <= s0 (fees)
            assert(*newSharesTotal <= s1);
            auto result { *newSharesTotal - sharesTotal };
            sharesTotal = *newSharesTotal;
            return result;
        }
    }
    auto& liquidity() { return *(static_cast<PoolLiquidity_uint64*>(this)); }
    auto& liquidity() const { return *(static_cast<const PoolLiquidity_uint64*>(this)); }

    [[nodiscard]] BaseQuote_uint64 liquidity_equivalent(NonzeroFunds_uint64 shares) const
    {
        assert(shares.value() <= sharesTotal);
        Nonzero_uint64 totalShares(sharesTotal); // At this point we know that sharesTotal > 0
        auto b { Prod128(shares, base).divide_floor(totalShares) };
        assert(b.has_value());
        auto q { Prod128(shares, quote).divide_floor(totalShares) };
        assert(q.has_value());
        return { *b, *q };
    }
    [[nodiscard]] std::optional<BaseQuote_uint64> withdraw_liquity(NonzeroFunds_uint64 shares)
    {
        if (sharesTotal < shares.value())
            return {};
        auto le { liquidity_equivalent(shares) };
        sharesTotal -= shares.value();
        base.subtract_assert(le.base);
        quote.subtract_assert(le.quote);
        return le;
    }

    auto base_total() const { return base; }
    auto quote_total() const { return quote; }
    auto shares_total() const { return sharesTotal; }

private:
    void withdraw(BaseQuote_uint64 liquidity, uint64_t shares)
    {
        assert(sharesTotal >= shares);
        base.subtract_assert(liquidity.base);
        quote.subtract_assert(liquidity.quote);
        sharesTotal -= shares;
    }

private:
    uint64_t sharesTotal { 0 };
};

} // namespace defi
