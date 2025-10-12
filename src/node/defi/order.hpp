#pragma once

#include "block/body/transaction_id.hpp"
#include "block/chain/history/index.hpp"
#include "defi/uint64/types.hpp"
#include "general/funds.hpp"

struct OrderData {
    HistoryId id;
    TransactionId txid;
    defi::Order_uint64 order;
    Funds_uint64 filled;
    Funds_uint64 remaining() const
    {
        return diff_assert(order.amount, filled);
    }
    OrderData(HistoryId id, TransactionId txid,
        Funds_uint64 amount, Funds_uint64 filled, Price_uint64 limit)
        : id(id)
        , txid(txid)
        , order { amount, limit }
        , filled(filled)
    {
        if (filled > amount)
            throw std::runtime_error("Order fill value cannot exceeed total amount");
    }
};
