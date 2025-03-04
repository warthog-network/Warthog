#pragma once
#include "matcher.hpp"
#include "sorted_order_vector.hpp"

namespace defi {
class Orderbook_uint64 {

public:
    [[nodiscard]] MatchResult_uint64 match(const PoolLiquidity_uint64& p) const;
    void insert_largest_base(Order_uint64 o)
    {
        return pushBaseAsc.insert_largest_asc(std::move(o));
    }
    void insert_base(Order_uint64 o)
    {
        return pushBaseAsc.insert_asc(std::move(o));
    }
    void insert_smallest_quote(Order_uint64 o)
    {
        pushQuoteDesc.insert_smallest_desc(std::move(o));
    }
    void insert_quote(Order_uint64 o)
    {
        pushQuoteDesc.insert_desc(std::move(o));
    }
    bool delete_quote(size_t i)
    {
        if (pushQuoteDesc.delete_at(i)) {
            return true;
        }
        return false;
    }
    bool delete_base(size_t i)
    {
        if (pushBaseAsc.delete_at(pushBaseAsc.size() - 1 - i)) {
            return true;
        }
        return false;
    }
    auto& quote_desc_buy() const { return pushQuoteDesc; }
    auto& base_asc_sell() const { return pushBaseAsc; }

private:
    SortedOrderVector_uint64 pushQuoteDesc; // limit price DESC (buy)
    SortedOrderVector_uint64 pushBaseAsc; // limit price ASC (sell)
};

} // namespace defi
