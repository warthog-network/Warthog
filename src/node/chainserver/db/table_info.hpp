#pragma once

#include "block/body/account_id.hpp"
#include "block/chain/history/index.hpp"
#include "defi/token/id.hpp"

#define BLOCKS_TABLE "Blocks"
#define CONSENSUS_TABLE "Consensus"
#define BADBLOCKS_TABLE "Badblocks"
#define DELETESCHEDULE_TABLE "Deleteschedule"
#define FILLLINK_TABLE "FillLink"

// tables that are pruned with state32, using it as as globally unique id
#define ASSETS_TABLE "Assets"
#define ACCOUNTS_TABLE "Accounts"

// tables that are pruned with state64, using it as as globally unique id
#define BALANCES_TABLE "Balances"
#define TOKENFORKBALANCES_TABLE "TokenForkBalances"
#define SELLORDERS_TABLE "SellOrders"
#define BUYORDERS_TABLE "BuyOrders"
#define POOLS_TABLE "Pools"
#define CANCELED_TABLE "Canceled"
#define PEG_TABLE "Pegs"

// tables that are pruned with historyId as globally unique id
#define ACCOUNTHISTORY_TABLE "AccountHistory"
#define HISTORY_TABLE "History"

namespace table_info {
template <typename T>
struct TableName {
    static_assert(false);
};

template <>
struct TableName<BalanceId> {
    static constexpr const char name[] = BALANCES_TABLE;
};

template <>
struct TableName<AccountId> {
    static constexpr const char name[] = ACCOUNTS_TABLE;
};

template <>
struct TableName<AssetId> {
    static constexpr const char name[] = ASSETS_TABLE;
};
template <>
struct TableName<HistoryId> {
    static constexpr const char name[] = HISTORY_TABLE;
};
template <>
struct TableName<TokenForkBalanceId> {
    static constexpr const char name[] = TOKENFORKBALANCES_TABLE;
};

template <typename T>
[[nodiscard]] constexpr auto table_name()
{
    return TableName<T>::name;
}
};
