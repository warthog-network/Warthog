#include "matching.hpp"
#include "pool.hpp"
namespace defi {
class Matcher : public Evaluator {
public:
  Matcher(BuySellOrders &bso, Pool &p)
      : Evaluator(p.base_total(), 0, p.quote_total(),
                  0), // TODO: initialize with pool
        toPool0{false, bso.pushBaseAsc.total_push()},
        toPool1{true, bso.pushQuoteDesc.total_push()} {}
  Delta orderExcess(Price p) {
    if (quoteIntoPool1 == true) {
      auto q{multiply_floor(in.base, p)};
      if (q.has_value() && *q <= in.quote) // too much base
        return {true, in.quote - *q};
    }
    auto b{divide_floor(in.quote, p)};
    assert(b.has_value()); // TODO: verify assert by checking precision of
                           // divide_floor
    assert(*b <= in.base);
    return {false, in.base - *b};
  }
  bool needs_increase(Price p) {
    auto [isQuote, toPool]{orderExcess(p)};
    bool needsIncrease{
        isQuote ? rel_quote_price(toPool, p) != std::strong_ordering::greater
                : rel_base_price(toPool, p) == std::strong_ordering::less};
    if (needsIncrease)
      toPool0 = {isQuote, toPool};
    else
      toPool1 = {isQuote, toPool};
    return needsIncrease;
  };

  FillResult bisect_dynamic_price() {
    auto bisect = [](Evaluator::ret_t::Fill64Ratio ratio0, uint64_t v1,
                     auto asc_fun) {
      using ret_t = Evaluator::ret_t;
      if (v1 == 0)
        return v1;
      ret_t r{asc_fun(v1)};
      assert(r.rel != std::strong_ordering::less);
      if (r.rel == std::strong_ordering::equal)
        return v1;
      auto ratio1{r.get_exceeded_ratio()};
      uint64_t v0{0};
      while (v0 + 1 < v1) {
        uint64_t v{(v1 + v0) / 2};
        ret_t ret = asc_fun(v);
        if (ret.exceeded()) {
          v1 = v;
          ratio1 = ret.get_exceeded_ratio();
        } else {
          v0 = v;
          ratio0 = ret.get_unexceeded_ratio();
        }
      }
      if (ratio1.b * ratio0.a < ratio1.a * ratio0.b)
        return v0;
      return v1;
    };
    auto baseRet{rel_base_asc(0)};
    auto quoteRet{rel_quote_asc(0)};
    if (baseRet.rel == std::strong_ordering::greater) {
      // need to push quote to pool
      assert(quoteRet.rel != std::strong_ordering::greater); // TODO: verify
      auto toPool{
          bisect(quoteRet.get_unexceeded_ratio(), in.quote,
                 [&](uint64_t toPool) { return rel_quote_asc(toPool); })};
      return {{true, toPool}, {}, in};
    } else {
      assert(baseRet.rel != std::strong_ordering::greater); // TODO: verify
      // need to push base to pool
      auto toPool{
          bisect(baseRet.get_unexceeded_ratio(), in.base,
                 [&](uint64_t toPool) { return rel_base_asc(toPool); })};
      return {{false, toPool}, {}, in};
    }
  }
  FillResult bisect_fixed_price(const bool isQuoteOrder, const uint64_t order0,
                                const uint64_t order1, Price p) {
    auto v0{order0};
    auto v1{order1};
    auto &v{isQuoteOrder ? in.quote : in.base};
    while (v1 + 1 != v0 && v0 + 1 != v1) {
      v = (v0 + v1) / 2;
      if (needs_increase(p))
        v0 = v;
      else
        v1 = v;
    }

    v = (toPool0.isQuote? v0 : v1);
    auto toPool{toPool0.isQuote? toPool0 : toPool1};
    auto nf{std::max(order0, order1) - v};
    std::optional<Delta> notFilled;
    if (nf>0) {
        notFilled = Delta{isQuoteOrder, nf};
    }
    auto filled{isQuoteOrder ?BaseQuote{in.base,v}: BaseQuote{v,in.quote} };

    return {.toPool{toPool}, .notFilled{notFilled}, .filled{filled}};
  };
  bool quoteIntoPool1{true};
  Delta toPool0;
  Delta toPool1;
};
auto BuySellOrders::match(Pool &p) -> MatchResult {
  const size_t I{pushQuoteDesc.size()};
  const size_t J{pushBaseAsc.size()};
  size_t i0{0};
  size_t i1{I};
  Matcher m{*this, p};

  while (i0 != i1) {
    auto i{(i0 + i1) / 2};
    auto &eq{extraQuote[i]};
    auto j{eq.upperBoundCounterpart};
    m.in.base = (j == J ? 0 : extraBase[j].cumsum);
    m.in.quote = eq.cumsum;
    if (m.needs_increase(pushQuoteDesc[i].limit))
      i0 = i + 1;
    else
      i1 = i;
  }
  auto bisect_j = [&](size_t j0, size_t j1) -> MatchResult {
    while (j0 != j1) {
      auto j{(j0 + j1) / 2};
      m.in.base = extraBase[j].cumsum;
      if (m.needs_increase(pushBaseAsc[J - 1 - j].limit))
        j0 = j + 1;
      else
        j1 = j;
    }
    if (j1 == 0) {
      return {m.bisect_dynamic_price(), i1, J - j1};
    } else {
      auto j{j1 - 1};
      m.in.base = extraBase[j].cumsum - pushBaseAsc[J - 1 - j].amount;
      auto price{pushBaseAsc[J - 1 - j].limit};
      if (m.needs_increase(price)) {
        return {m.bisect_dynamic_price(), i1, J - j1};
      } else {
        return {
            m.bisect_fixed_price(false, extraBase[j].cumsum, m.in.base, price),
            i1, J - j1};
      }
    }
  };
  if (i1 == 0) {
    size_t j0 = 0;
    size_t j1 = (I == 0 ? J : extraQuote[0].upperBoundCounterpart);
    m.in.quote = 0;
    return bisect_j(j0, j1);
  } else {
    auto i{i1 - 1};
    auto &eq{extraQuote[i]};
    auto price{pushQuoteDesc[i].limit};
    auto j{eq.upperBoundCounterpart};
    m.in.base = (j == J ? 0 : extraBase[j].cumsum);
    m.in.quote = eq.cumsum + pushQuoteDesc[i].amount;
    size_t j0 = extraQuote[i].upperBoundCounterpart;
    if (m.needs_increase(price)) {
      size_t j1 = (i1 < I ? extraQuote[i1].upperBoundCounterpart : J);
      return bisect_j(j0, j1);
    } else {
      return {m.bisect_fixed_price(true, eq.cumsum, m.in.quote, price), i,
              J - j0};
    }
  }
}
} // namespace defi
