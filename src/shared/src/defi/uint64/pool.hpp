#pragma once
#include "../price.hpp"
#include "src/defi/uint64/types.hpp"
#include <cstdint>

namespace defi {
struct PoolLiquidity_uint64 : public BaseQuote_uint64 {
    using BaseQuote_uint64::BaseQuote_uint64;

    Ratio128 price_ratio_added_quote(uint64_t quoteToPool) const
    {
        return {
            .numerator { Prod128(quote + quoteToPool, quote + quoteToPool) },
            .denominator { Prod128(quote, base) }
        };
    }

    Ratio128 price_ratio_added_base(uint64_t baseToPool) const
    {
        return {
            .numerator { Prod128(base, quote) },
            .denominator { Prod128(base + baseToPool, base + baseToPool) }
        };
    }

    // relation of pool price (affected by adding given quoteToPool) to given price
    [[nodiscard]] std::strong_ordering rel_quote_price(uint64_t quoteToPool,
        Price p) const
    {
        return compare_fraction(price_ratio_added_quote(quoteToPool), p);
    }

    // relation of pool price (affected by pushing given baseToPool) to given price
    [[nodiscard]] std::strong_ordering rel_base_price(uint64_t baseToPool,
        Price p) const
    {
        return compare_fraction(price_ratio_added_base(baseToPool), p);
    }

    [[nodiscard]] bool modified_pool_price_exceeds(const Delta_uint64& toPool, Price p) const
    {
        if (toPool.isQuote)
            return rel_quote_price(toPool.amount, p) == std::strong_ordering::greater;
        else
            return rel_base_price(toPool.amount, p) != std::strong_ordering::less;
    }
};

class Pool_uint64 : public PoolLiquidity_uint64{
public:
  Pool_uint64(uint64_t base, uint64_t quote)
      : PoolLiquidity_uint64(base, quote) {
        assert(base!=0 && quote!=0);
    }
  struct Tokens {
    uint64_t val;
  };


  [[nodiscard]] uint64_t sell(uint64_t baseAdd, uint64_t feeE4 = 10) {
    auto quoteDelta = swapped_amount(base, baseAdd, quote, feeE4);
    quote -= quoteDelta;
    base += baseAdd;
    return quoteDelta;
  }

  [[nodiscard]] uint64_t buy(uint64_t quoteAdd, uint64_t feeE4 = 10) {
    auto baseDelta = swapped_amount(quote, quoteAdd, base, feeE4);
    base -= baseDelta;
    quote += quoteAdd;
    return baseDelta;
  }
  [[nodiscard]] uint64_t deposit(uint64_t base, uint64_t quote) {
    auto s0{Prod128(base, quote).sqrt()};
    base += base;
    quote += quote;
    auto s1{Prod128(base, quote).sqrt()};
    assert(s1 >= s0);
    auto newTokensTotal{Prod128(tokensTotal, s1).divide_floor(s0)};
    assert(newTokensTotal.has_value()); // no overflow because tokensTotal <= s0
    assert(*newTokensTotal <= s1);
    auto result{*newTokensTotal - tokensTotal};
    tokensTotal = *newTokensTotal;
    return result;
  }
  auto base_total() const { return base; }
  auto quote_total() const { return quote; }

private:
  static uint64_t discount(uint64_t value, uint16_t feeE4) {
    if (value == 0)
      return 0;
    auto e = std::countl_zero(value);
    value <<= e;
    auto d{value / 10000};
    bool exact{d * 10000 == value};
    value -= d * feeE4;
    if (!exact && value > 0)
      value -= 1;
    return value >> e;
  }
  static uint64_t swapped_amount(uint64_t a0, uint64_t a_add, uint64_t b0,
                                 uint64_t feeE4) {
    auto bnew{Prod128(a0, b0).divide_ceil(a0 + a_add)};
    assert(bnew.has_value()); // cannot overflow
    const auto b1{*bnew};
    assert(b1 <= b0);
    uint64_t bDelta{b0 - b1};
    return discount(bDelta, feeE4);
  }

private:
  uint64_t tokensTotal;
};


} // namespace defi
