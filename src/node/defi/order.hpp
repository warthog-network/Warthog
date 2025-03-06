#pragma once

#include "block/body/account_id.hpp"
#include "block/body/order_id.hpp"
#include "defi/uint64/types.hpp"
#include "general/funds.hpp"

struct Order {
    Price_uint64 price;
    Funds amount;
};

struct OrderData {
    OrderId id;
    AccountId aid;
    Order order;
    Funds filled;
    OrderData(OrderId id, AccountId aid, Funds amount, Funds filled, Price_uint64 p)
        : id(id)
        , aid(aid)
        , order { p, amount }
        , filled(filled)
    {
        if (filled > amount)
            throw std::runtime_error("Order fill value cannot exceeed total amount");
    }
};
