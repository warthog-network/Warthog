#pragma once
#include "communication/rxtx_server/api_types_fwd.hpp"
#include "tools/variant_fwd.hpp"
namespace api {
struct AccountHistory;
struct AccountIdOrAddress;
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
namespace block {
struct Reward;
struct WartTransfer;
struct TokenTransfer;
struct AssetCreation;
struct NewOrder;
struct Match;
struct LiquidityDeposit;
struct LiquidityWithdrawal;
struct Cancelation;
};
template <typename TxType>
struct Temporal;
using RewardTransaction = Temporal<block::Reward>;
using WartTransferTransaction = Temporal<block::WartTransfer>;
using TokenTransferTransaction = Temporal<block::TokenTransfer>;
using AssetCreationTransaction = Temporal<block::AssetCreation>;
using NewOrderTransaction = Temporal<block::NewOrder>;
using MatchTransaction = Temporal<block::Match>;
using LiquidityDepositTransaction = Temporal<block::LiquidityDeposit>;
using LiquidityWithdrawalTransaction = Temporal<block::LiquidityWithdrawal>;
using CancelationTransaction = Temporal<block::Cancelation>;
using Transaction = wrt::variant<
    RewardTransaction,
    WartTransferTransaction,
    TokenTransferTransaction,
    AssetCreationTransaction,
    NewOrderTransaction,
    MatchTransaction,
    LiquidityDepositTransaction,
    LiquidityWithdrawalTransaction,
    CancelationTransaction>;

struct Richlist;
struct Rollback;
struct Round16Bit;
struct TransactionsByBlocks;
struct Wallet;
struct DBSize;
struct NodeInfo;
struct IPCounter;
struct ThrottleState;
struct ThrottledPeer;
}
