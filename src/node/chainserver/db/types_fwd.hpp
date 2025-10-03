#pragma once

namespace chain_db {
class ChainDB;
class ChainDBTransaction;
struct AssetData;
struct OrderDelete;
struct OrderFillstate;
struct AccountData;
struct BalanceData;
struct TokenForkBalanceData;
struct OrderData;
struct PoolData;
}

using ChainDB = chain_db::ChainDB;
using ChainDBTransaction = chain_db::ChainDBTransaction;
