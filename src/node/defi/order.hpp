#pragma once

#include "block/body/account_id.hpp"
#include "block/body/order_id.hpp"
#include "defi/uint64/types.hpp"
#include "general/funds.hpp"

struct OrderData {
    OrderId id;
    AccountId aid;
    defi::Order_uint64 order;
    Funds_uint64 filled;
    Funds_uint64 remaining() const
    {
        return Funds_uint64::diff_assert(order.amount, filled);
    }
    OrderData(OrderId id, AccountId aid, Funds_uint64 amount, Funds_uint64 filled, Price_uint64 p)
        : id(id)
        , aid(aid)
        , order { amount, p }
        , filled(filled)
    {
        if (filled > amount)
            throw std::runtime_error("Order fill value cannot exceeed total amount");
    }
};
