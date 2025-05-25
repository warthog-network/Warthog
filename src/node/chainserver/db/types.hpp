#pragma once

#include "block/body/account_id.hpp"
#include "block/body/transaction_id.hpp"
#include "block/chain/height.hpp"
#include "block/chain/history/index.hpp"
#include "defi/token/id.hpp"
#include "defi/token/token.hpp"
#include "defi/uint64/price.hpp"
#include "types_fwd.hpp"
namespace chain_db {
struct TokenData {
    TokenId id;
    NonzeroHeight height;
    AccountId ownerAccountId;
    FundsDecimal supply;
    TokenId groupId;
    TokenId parentId;
    TokenName name;
    TokenHash hash;
    std::vector<uint8_t> data;
};
struct OrderDelete {
    HistoryId id;
    bool buy;
};
struct OrderFillstate {
    HistoryId id;
    bool buy;
    Funds_uint64 filled;
};
struct OrderData {
    HistoryId id;
    bool buy;
    TransactionId txid;
    TokenId tid;
    Funds_uint64 total;
    Funds_uint64 filled;
    Price_uint64 limit;
};
}
