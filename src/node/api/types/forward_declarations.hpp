#pragma once
#include <variant>
#include "communication/rxtx_server/api_types_fwd.hpp"
namespace api {
struct AccountHistory;
struct AccountIdOrAddress;;
struct AddressCount;
struct AddressWithId;
struct WartBalance;
struct Block;
struct CompleteBlock;
struct ChainHead;
struct HashrateBlockChart;
struct HashrateChartRequest;
struct HashrateInfo;
struct HashrateTimeChart;
struct Head;
struct HeightOrHash;
struct MempoolEntries;
struct MiningState;
struct Peerinfo;
struct PeerinfoConnections;
struct Raw;
struct RewardTransaction;
struct Richlist;
struct Rollback;
struct Round16Bit;
struct TransactionsByBlocks;
struct TransferTransaction;
struct Wallet;
struct DBSize;
struct NodeInfo;
struct IPCounter;
struct ThrottleState;
struct ThrottledPeer;
using Transaction = std::variant<RewardTransaction, TransferTransaction>;
}
