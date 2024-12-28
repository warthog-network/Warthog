#pragma once
#include<cstdint>
#include "price.hpp"

namespace defi {
struct BuyTx {
  uint64_t sellQuote;
};
struct BuyResult {
  uint64_t payQuote;
  uint64_t receiveBase;
};
struct SellTx {
  uint64_t sellBase;
};
struct SellResult {
  uint64_t payBase;
  uint64_t receiveQuote;
};
class Pool {

public:
  Pool(uint64_t base, uint64_t quote) : baseTotal(base), quoteTotal(quote) {}
  struct Tokens {
    uint64_t val;
  };
  BuyResult apply(BuyTx tx) {
    return {.payQuote = tx.sellQuote, .receiveBase = buy(tx.sellQuote)};
  }
  SellResult apply(SellTx tx) {
    return {.payBase = tx.sellBase, .receiveQuote = buy(tx.sellBase)};
  }
  struct MultiApplyResult {};

  PriceRelative price() {
    return PriceRelative::from_fraction(quoteTotal, baseTotal);
  }

  [[nodiscard]] uint64_t sell(uint64_t baseAdd, uint64_t feeE4 = 10) {
    auto quoteDelta = swapped_amount(baseTotal, baseAdd, quoteTotal, feeE4);
    quoteTotal -= quoteDelta;
    baseTotal += baseAdd;
    return quoteDelta;
  }

  [[nodiscard]] uint64_t buy(uint64_t quoteAdd, uint64_t feeE4 = 10) {
    auto baseDelta = swapped_amount(quoteTotal, quoteAdd, baseTotal, feeE4);
    baseTotal -= baseDelta;
    quoteTotal += quoteAdd;
    return baseDelta;
  }
  [[nodiscard]] uint64_t deposit(uint64_t base, uint64_t quote) {
    auto s0{Prod128(baseTotal, quoteTotal).sqrt()};
    baseTotal += base;
    quoteTotal += quote;
    auto s1{Prod128(baseTotal, quoteTotal).sqrt()};
    assert(s1 >= s0);
    auto newTokensTotal{Prod128(tokensTotal, s1).divide_floor(s0)};
    assert(newTokensTotal.has_value()); // no overflow because tokensTotal <= s0
    assert(*newTokensTotal <= s1);
    auto result{*newTokensTotal - tokensTotal};
    tokensTotal = *newTokensTotal;
    return result;
  }
  auto base_total() const { return baseTotal; }
  auto quote_total() const { return quoteTotal; }

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
  uint64_t baseTotal;
  uint64_t quoteTotal;
  uint64_t tokensTotal;
};
}
