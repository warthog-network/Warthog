#pragma once

#include "block/body/account_id.hpp"
#include "block/chain/history/index.hpp"
#include "defi/token/id.hpp"

#define BALANCES_TABLE "Balances"
#define BLOCKS_TABLE "Blocks"
#define ACCOUNTHISTORY_TABLE "AccountHistory"
#define SELLORDERS_TABLE "SellOrders"
#define BUYORDERS_TABLE "BuyOrders"
#define CANCELED_TABLE "Canceled"
#define POOLS_TABLE "Pools"
#define PEG_TABLE "Pegs"
#define TOKENFORKBALANCES_TABLE "TokenForkBalances"
#define ASSETS_TABLE "Assets"
#define BALANCES_TABLE "Balances"
#define CONSENSUS_TABLE "Consensus"
#define ACCOUNTS_TABLE "Accounts"
#define BADBLOCKS_TABLE "Badblocks"
#define DELETESCHEDULE_TABLE "Deleteschedule"
#define FILLLINK_TABLE "FillLink"
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
template <typename T>
[[nodiscard]] constexpr auto table_name()
{
    return TableName<T>::name;
}
};
