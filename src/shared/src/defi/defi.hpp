#pragma once
#include "general/funds.hpp"
#include "uint64/orderbook.hpp"
#include "uint64/pool.hpp"
namespace defi {

struct Order {
    Funds amount;
    Price_uint64 limit;
};

class Pool {
    friend class BuySellOrders;
    struct BuyTx {
        Funds sellQuote;
    };
    struct BuyResult {
        Funds payQuote;
        Funds receiveBase;
    };

    struct SellTx {
        Funds sellBase;
    };
    struct SellResult {
        Funds payBase;
        Funds receiveQuote;
    };

public:
    Pool(Funds base, Funds quote)
        : pool_uint64(base.E8(), quote.E8())
    {
    }
    Funds base_total() const
    {
        return Funds::from_value_throw(pool_uint64.base_total());
    }
    Funds quote_total() const
    {
        return Funds::from_value_throw(pool_uint64.quote_total());
    }
    auto price() const { return pool_uint64.price(); }

    Funds sell(Funds baseAdd, uint64_t feeE4 = 10)
    {
        return Funds::from_value_throw(pool_uint64.sell(baseAdd.E8(), feeE4));
    }
    Funds buy(Funds quoteAdd, uint64_t feeE4 = 10)
    {
        return Funds::from_value_throw(pool_uint64.buy(quoteAdd.E8(), feeE4));
    }

    BuyResult apply(BuyTx tx)
    {
        return { .payQuote = tx.sellQuote,
            .receiveBase = Funds::from_value_throw(pool_uint64.buy(tx.sellQuote.E8())) };
    }
    SellResult apply(SellTx tx)
    {
        return { .payBase = tx.sellBase,
            .receiveQuote = Funds::from_value_throw(pool_uint64.sell(tx.sellBase.E8())) };
    }

private:
    Pool_uint64 pool_uint64;
};

struct BaseQuote;
struct Delta {
    Delta(Delta_uint64 d)
        : d(std::move(d))
    {
    }
    Funds amount() const { return Funds::from_value_throw(d.amount); }
    bool is_quote() const { return d.isQuote; }
    bool is_base() const { return !is_quote(); }
    BaseQuote base_quote() const;

private:
    Delta_uint64 d;
};

struct BaseQuote {
    BaseQuote(BaseQuote_uint64 bq)
        : base(Funds::from_value_throw(bq.base))
        , quote(Funds::from_value_throw(bq.quote))
    {
    }
    BaseQuote(Funds base, Funds quote)
        : base(base)
        , quote(quote)
    {
    }
    BaseQuote()
        : BaseQuote(Funds::zero(), Funds::zero())
    {
    }
    Funds base;
    Funds quote;
    BaseQuote operator-(const BaseQuote& other)
    {
        return { base.subtract_throw(other.base), quote.subtract_throw(other.quote) };
    }
    BaseQuote& add_throw(const BaseQuote& bq)
    {
        base.add_throw(bq.base);
        quote.add_throw(bq.quote);
        return *this;
    }
    BaseQuote& subtract_throw(const BaseQuote& bq)
    {
        base.subtract_throw(bq.base);
        quote.subtract_throw(bq.quote);
        return *this;
    }
    auto& subtract_throw(const Delta& d)
    {
        return subtract_throw(d.base_quote());
    }
    [[nodiscard]] std::optional<double> price_double() const
    {
        if (base.is_zero())
            return {};
        return double(quote.E8()) / double(base.E8());
    }
    auto price() const
    {
        return PriceRelative::from_fraction(quote.E8(), base.E8());
    }
};

class MatchResult {
public:
    MatchResult(MatchResult_uint64 mr)
        : mr(std::move(mr))
    {
    }
    std::optional<Delta> to_pool() const
    {
        if (mr.toPool)
            return Delta(*mr.toPool);
        return {};
    }
    BaseQuote filled() const { return mr.filled; }

private:
    MatchResult_uint64 mr;
};

inline BaseQuote Delta::base_quote() const
{
    if (is_quote())
        return { Funds::zero(), amount() };
    return { amount(), Funds::zero() };
}

class BuySellOrders {
    class OrderView {
    public:
        Funds amount() const
        {
            return Funds::from_value_throw(order_uint64.amount);
        }
        auto limit() const { return order_uint64.limit; }
        OrderView(const Order_uint64& o)
            : order_uint64(o) { };

    private:
        const Order_uint64& order_uint64;
    };

    class OrderVecView {
    public:
        OrderVecView(const SortedOrderVector_uint64& ov)
            : ov(ov)
        {
        }
        OrderView operator[](size_t i) const { return ov[i]; }
        auto size() const { return ov.size(); }

    private:
        const SortedOrderVector_uint64& ov;
    };

public:
    [[nodiscard]] MatchResult match_assert_lazy(Pool& p);
    auto insert_base(Order o)
    {
        return buySellOrders_uint64.insert_base(
            { .amount = o.amount.E8(), .limit { o.limit } });
    }
    auto insert_quote(Order o)
    {
        return buySellOrders_uint64.insert_quote(
            { .amount = o.amount.E8(), .limit { o.limit } });
    }
    bool delete_index(bool base, size_t i)
    {
        if (base)
            return buySellOrders_uint64.delete_base(i);
        return buySellOrders_uint64.delete_quote(i);
    }
    OrderVecView quote_desc_buy() const
    {
        return buySellOrders_uint64.quote_desc_buy();
    }
    OrderVecView base_asc_sell() const
    {
        return buySellOrders_uint64.base_asc_sell();
    }

private:
    Orderbook_uint64 buySellOrders_uint64;
};

} // namespace defi
