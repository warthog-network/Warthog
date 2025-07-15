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

struct RewardData;
struct WartTransferData;
struct TokenTransferData;
struct AssetCreationData;
struct NewOrderData;
struct LiquidityDepositData;
struct LiquidityWithdrawalData;
struct CancelationData;
struct MatchData;

template <typename T>
struct WithHistoryBase;
template <typename T>
struct WithSignedInfo;

using Reward = WithHistoryBase<RewardData>;
using WartTransfer = WithSignedInfo<WartTransferData>;
using TokenTransfer = WithSignedInfo<TokenTransferData>;
using AssetCreation = WithSignedInfo<AssetCreationData>;
using NewOrder = WithSignedInfo<NewOrderData>;
using LiquidityDeposit = WithSignedInfo<LiquidityDepositData>;
using LiquidityWithdrawal = WithSignedInfo<LiquidityWithdrawalData>;
using Cancelation = WithSignedInfo<CancelationData>;
using Match = WithHistoryBase<MatchData>;


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
