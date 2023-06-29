#pragma once
#include "block/chain/height.hpp"
#include "block/body/transaction_id.hpp"
namespace mempool{
struct OrderKey {
    Height transactionHeight;
    TransactionId txid;
    auto operator<=>(const OrderKey& h)const = default;
};

}
