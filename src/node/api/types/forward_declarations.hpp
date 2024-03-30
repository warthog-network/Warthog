#pragma once
#include <variant>
namespace API {
struct MempoolEntries;
struct TransferTransaction;
struct Head;
struct RewardTransaction;
struct Balance;
struct HashrateInfo;
struct Block;
struct AccountHistory;
struct TransactionsByBlocks;
struct HashrateChart;
struct HashrateChartRequest;
struct Richlist;
struct Peerinfo;
struct HeightOrHash;
struct Round16Bit;
struct PeerinfoConnections;
struct AccountIdOrAddress;;
struct Wallet;
struct Rollback;
struct Raw;
using Transaction = std::variant<RewardTransaction, TransferTransaction>;
}
