#pragma once
#include "order.hpp"
#include "price.hpp"
#include <cstdint>
#include <variant>
#include <vector>
namespace defi {
class Pool;
class Matcher;

struct BaseQuote;
struct Delta {
  bool isQuote{false};
  uint64_t amount;
  BaseQuote base_quote() const;
};
struct BaseQuote {
  uint64_t base;
  uint64_t quote;
  BaseQuote operator-(const Delta &bq) const {
    auto res{*this};
    if (bq.isQuote)
      res.quote -= bq.amount;
    else
      res.base -= bq.amount;
    return res;
  }
  auto price() const { return PriceRelative::from_fraction(quote, base); }
};
inline BaseQuote Delta::base_quote() const{
    if (isQuote) 
        return {0,amount};
    return {amount,0};
}

struct FillResult {
  Delta toPool;
  std::optional<Delta> notFilled;
  BaseQuote filled;
};
struct MatchResult: public FillResult {
    size_t quoteBound; 
    size_t baseBound;
};
 
class Evaluator {
  static std::pair<std::strong_ordering, Prod192> take_smaller(Prod192 &a,
                                                               Prod192 &b) {
    auto rel{a <=> b};
    if (rel == std::strong_ordering::less)
      return {rel, a};
    return {rel, b};
  }

public:
  Evaluator(uint64_t basePool, uint64_t baseIn, uint64_t quotePool,
            uint64_t quoteIn)
      : in{baseIn, quoteIn}, pool{basePool, quotePool} {}

  BaseQuote in;
  BaseQuote pool;
  struct ret_t {
    std::strong_ordering rel;
    struct Pool128Ratio {
      Prod128 a, b;
    };
    struct Fill64Ratio {
      uint64_t a, b;
    };
    bool exceeded() const { return std::holds_alternative<Pool128Ratio>(v); }
    auto &get_unexceeded_ratio() const { return std::get<Fill64Ratio>(v); };
    auto &get_exceeded_ratio() const { return std::get<Pool128Ratio>(v); };
    std::variant<Pool128Ratio, Fill64Ratio> v;
  };

  [[nodiscard]] ret_t rel_base_asc(uint64_t baseDelta) const {
    auto pp1{Prod128(pool.base, pool.quote)};
    auto pp2{in.base - baseDelta};
    auto pp{pp1 * pp2};
    auto op1{Prod128(pool.base + baseDelta, pool.base + baseDelta)};
    auto op2{in.quote};
    auto op{op1 * op2};

    auto rel{op <=> pp};
    if (rel == std::strong_ordering::greater) {
      return {rel, ret_t::Pool128Ratio{pp1, op1}};
    }
    return {rel, ret_t::Fill64Ratio{pp2, op2}};
  }
  [[nodiscard]] ret_t rel_quote_asc(uint64_t quoteToPool) const {
    auto pp1{Prod128(pool.quote, pool.base)};
    auto pp2{in.quote - quoteToPool};
    auto pp{pp1 * pp2};
    auto op1{Prod128(pool.quote + quoteToPool, pool.quote + quoteToPool)};
    auto op2{in.base};
    auto op{op1 * op2};
    auto rel{op <=> pp};
    if (rel == std::strong_ordering::greater) {
      return {rel, ret_t::Pool128Ratio{pp1, op1}};
    }
    return {rel, ret_t::Fill64Ratio{pp2, op2}};
  }

  [[nodiscard]] std::strong_ordering rel_quote_price(uint64_t quoteToPool,
                                                     Price p) const {
    return compare_fraction(
        Prod128(pool.quote + quoteToPool, pool.quote + quoteToPool),
        Prod128(pool.base, pool.quote), p);
  }
  [[nodiscard]] std::strong_ordering rel_base_price(uint64_t baseToPool,
                                                    Price p) const {
    return compare_fraction(
        Prod128(pool.base, pool.quote),
        Prod128(pool.base + baseToPool, pool.base + baseToPool), p);
  }
  [[nodiscard]] Prod192 quotedelta_quadratic_asc(uint64_t quoteDelta) const {
    return Prod128(pool.quote + quoteDelta, pool.quote + quoteDelta) * in.base;
  }
};

namespace ordervec {
struct elem_t : public Order {
  elem_t(Order o) : Order(std::move(o)) {}

  auto operator<=>(const elem_t &o) const {
    return limit.operator<=>(o.limit);
  };
  auto operator==(const elem_t &o) const { return limit == o.limit; }
};

struct Ordervec : protected std::vector<elem_t> {
  using parent_t = std::vector<elem_t>;
  Ordervec() {}
  using parent_t::size;
  using parent_t::operator[];
  void insert_desc(ordervec::elem_t o) {
    auto iter{std::lower_bound(rbegin(), rend(), o)};
    if (iter != rend() && *iter == o)
      iter->amount += o.amount;
    else
      insert(iter.base(), std::move(o));
    total += o.amount;
  }
  void insert_asc(ordervec::elem_t o) {
    auto iter{std::lower_bound(begin(), end(), o)};
    if (iter != end() && *iter == o)
      iter->amount += o.amount;
    else
      insert(iter, std::move(o));
    total += o.amount;
  }
  auto total_push() { return total; }

private:
  uint64_t total{0};
};

struct BuySellOrders {
  friend class defi::Matcher;
  struct ExtraData {
    uint64_t cumsum;
    size_t upperBoundCounterpart;
  };

  void prepare() {
    extraBase.resize(0);
    extraQuote.resize(0);
    size_t cumsumQuote{0};
    const size_t J{pushBaseAsc.size()};
    const size_t I{pushQuoteDesc.size()};
    uint64_t cumsumBase{pushBaseAsc.total_push()};
    size_t j{0};
    extraBase.resize(0);
    for (size_t i = 0; i < I; ++i) {
      auto &oq{pushQuoteDesc[i]};
      for (; j < J; ++j) {
        auto &ob{pushBaseAsc[J - 1 - j]};
        if (ob.limit <= oq.limit)
          break;
        extraBase.push_back({cumsumBase, i});
        cumsumBase -= ob.amount;
      }
      extraQuote.push_back({cumsumQuote, j});
      cumsumQuote += oq.amount;
    }
    for (; j < J; ++j) {
      extraBase.push_back({cumsumBase, I});
      cumsumBase -= pushBaseAsc[J - 1 - j].amount;
    }
  }
  struct Bisecter {
    /* data */
  };
  [[nodiscard]] MatchResult match(Pool &p);
  auto insert_base(elem_t e) {
    pushBaseAsc.insert_asc(e);
    prepare();
  }
  auto insert_quote(elem_t e) {
    pushQuoteDesc.insert_desc(e);
    prepare();
  }
  auto& quote_desc_buy() const {return pushQuoteDesc;}
  auto& base_asc_sell() const {return pushBaseAsc;}

private:
  Ordervec pushQuoteDesc; // limit price DESC (buy)
  std::vector<ExtraData> extraQuote;
  size_t dirtyQuote{0};

  Ordervec pushBaseAsc; // limit price ASC (sell)
  std::vector<ExtraData> extraBase;
  size_t dirtyBase{0};
};
} // namespace ordervec
using ordervec::BuySellOrders;

} // namespace defi
