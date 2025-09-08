#pragma once

#include "accountid_or_address.hpp"
#include "api/types/input.hpp"
#include "asset_lookup_trace.hpp"
#include "block/body/primitives.hpp"
#include "block/chain/history/index.hpp"
#include "block/chain/signed_snapshot.hpp"
#include "block/chain/worksum.hpp"
#include "block/header/difficulty_declaration.hpp"
#include "block/header/header.hpp"
#include "communication/mining_task.hpp"
#include "crypto/address.hpp"
#include "defi/token/token.hpp"
#include "defi/types.hpp"
#include "defi/uint64/pool.hpp"
#include "defi/uint64/price.hpp"
#include "eventloop/peer_chain.hpp"
#include "eventloop/types/conndata.hpp"
#include "forward_declarations.hpp"
#include "general/funds.hpp"
#include "general/start_time_points.hpp"
#include "height_or_hash.hpp"
#include "peerserver/db/offense_entry.hpp"
#include "transport/helpers/peer_addr.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
#include <vector>
namespace chainserver {
class DBCache;
}

namespace api {
using sc = std::chrono::steady_clock;
struct ChainHead {
    std::optional<SignedSnapshot> signedSnapshot;
    Worksum worksum;
    Target nextTarget;
    Hash hash;
    Height height;
    PinHash pinHash;
    PinHeight pinHeight;
    uint64_t hashrate;
};

struct MiningState {
    ChainMiningTask miningTask;
    bool synced;
};

struct Head {
    ChainHead chainHead;
    bool synced;
};

struct AddressWithId {
    Address address;
    AccountId accountId;
};
struct WartBalance {
    std::optional<AddressWithId> address;
    Wart balance;
    WartBalance()
        : balance(0)
    {
    }
};

struct TokenBalance {
    struct Lookup {
        AddressWithId address;
        AssetLookupTrace lookupFails;
    };
private:
    TokenBalance(std::optional<Lookup> lookup, FundsDecimal balance)
        : lookup(std::move(lookup))
        , balance(std::move(balance))
    {
    }
public:
    // data
    std::optional<Lookup> lookup;
    FundsDecimal balance;

    static TokenBalance notfound()
    {
        return { {}, FundsDecimal::zero() };
    }
    static TokenBalance found(const Address& addr, const AccountId& aid, AssetLookupTrace lookupTrace, FundsDecimal b)
    {
        return { Lookup{AddressWithId{addr,aid}, std::move(lookupTrace)}, std::move(b) };
    }
};

struct Rollback {
    static constexpr const char eventName[] = "rollback";
    Height length;
};

struct BlockSummary {
    Header header;
    NonzeroHeight height;
    uint32_t confirmations = 0;
    uint32_t nTransfers;
    Address miner;
    Wart transferred;
    Wart totalTxFee;
    Wart blockReward;
};
namespace block {
struct HistoryDataBase {
    TxHash txhash;
    std::optional<HistoryId> hid;
};

template <typename T>
struct WithHistoryBase : public HistoryDataBase, T {
    WithHistoryBase(TxHash txhash, HistoryId, T t)
        : HistoryDataBase(std::move(txhash), std::move(hid))
        , T(std::move(t))
    {
    }
};

struct SignedInfoData : public HistoryDataBase {
    SignedInfoData(TxHash txHash, std::optional<HistoryId> hid, Address originAddress, Wart fee, NonceId nonceId, PinHeight pinHeight)
        : HistoryDataBase(std::move(txHash), std::move(hid))
        , originAddress(std::move(originAddress))
        , fee(std::move(fee))
        , nonceId(std::move(nonceId))
        , pinHeight(std::move(pinHeight))
    {
    }
    Address originAddress;
    Wart fee;
    NonceId nonceId;
    PinHeight pinHeight;
};

template <typename T>
struct WithSignedInfo : public SignedInfoData, T {
    WithSignedInfo(SignedInfoData i, T t)
        : SignedInfoData(std::move(i))
        , T(std::move(t))
    {
    }
};

struct RewardData {
    static constexpr const char label[] = "Reward";
    Address toAddress;
    Wart wart;
};

struct WartTransferData {
    static constexpr const char label[] = "WartTransfer";
    Address toAddress;
    Wart amount;
};

struct TokenTransferData {
    static constexpr const char label[] = "TokenTransfer";
    AssetBasic assetInfo;
    Address toAddress;
    Funds_uint64 amount;
    FundsDecimal amount_decimal() const { return { amount, assetInfo.precision }; }
};

struct NewOrderData {
    static constexpr const char label[] = "Order";
    AssetBasic assetInfo;
    Funds_uint64 amount;
    Price_uint64 limit;
    bool buy;

    FundsDecimal amount_decimal() const { return { amount, buy ? assetInfo.precision : Wart::precision }; }
};

struct MatchData {
    static constexpr const char label[] = "Match";
    using Swap = CombineElements<BaseEl, QuoteEl, ReferredHistoryIdEl>;
    AssetBasic assetInfo;
    defi::BaseQuote poolBefore;
    defi::BaseQuote poolAfter;
    std::vector<Swap> buySwaps;
    std::vector<Swap> sellSwaps;
};

struct AssetCreationData {
    static constexpr const char label[] = "Creation";
    AssetName name;
    FundsDecimal supply;
    std::optional<AssetId> assetId;
};

struct CancelationData {
    static constexpr const char label[] = "Cancelation";
};

struct LiquidityDepositData {
    static constexpr const char label[] = "LiquidityDeposit";
    AssetBasic assetInfo;
    Funds_uint64 baseDeposited;
    Wart quoteDeposited;
    std::optional<Funds_uint64> sharesReceived;
};

struct LiquidityWithdrawalData {
    static constexpr const char label[] = "LiquidityWithdrawal";
    AssetBasic assetInfo;
    Funds_uint64 sharesRedeemed;
    std::optional<Funds_uint64> baseReceived;
    std::optional<Wart> quoteReceived;
};

struct Actions {
    std::optional<block::Reward> reward;
    std::vector<block::WartTransfer> wartTransfers;
    std::vector<block::TokenTransfer> tokenTransfers;
    std::vector<block::AssetCreation> assetCreations;
    std::vector<block::NewOrder> newOrders;
    std::vector<block::Match> matches;
    std::vector<block::LiquidityDeposit> liquidityDeposit;
    std::vector<block::LiquidityWithdrawal> liquidityWithdrawal;
    std::vector<block::Cancelation> cancelations;
};
}

struct Block {
    static constexpr const char eventName[] = "blockAppend";
    Header header;
    NonzeroHeight height;
    uint32_t confirmations = 0;

public:
    block::Actions actions;

    Block(Header header,
        NonzeroHeight height, uint32_t confirmations,
        block::Actions actions)
        : header(header)
        , height(height)
        , confirmations(confirmations)
        , actions(std::move(actions))
    {
    }
    void set_reward(block::Reward r);
};

struct CompleteBlock : public Block {
    explicit CompleteBlock(Block b)
        : Block(std::move(b))
    {
        if (!actions.reward)
            throw std::runtime_error("API Block is incomplete.");
    }
    auto& reward() const { return *actions.reward; }
};

struct TemporalInfo {
    uint32_t confirmations;
    Height height { 0 };
    uint32_t timestamp = 0;
};

template <typename TxType>
struct Temporal : public TemporalInfo, public TxType {
};

struct AddressCount {
    Address address;
    int64_t count;
};

struct AccountHistory {
    Wart balance;
    HistoryId fromId;
    std::vector<api::Block> blocks_reversed;
};
struct TransactionsByBlocks {
    size_t count { 0 };
    HistoryId fromId;
    std::vector<api::Block> blocks_reversed;
};
struct Richlist {
    std::vector<std::pair<Address, Wart>> entries;
};
struct MempoolEntry : public WartTransferMessage {
    TxHash txHash;
};
struct MempoolEntries {
    std::vector<MempoolEntry> entries;
};
struct HashrateInfo {
    size_t nBlocks;
    uint64_t estimate;
};

struct HashrateChartRequest {
    Height begin;
    Height end;
};

struct HashrateBlockChart {
    HashrateChartRequest range;
    std::vector<double> chart;
};

struct HashrateTimeChart {
    uint32_t begin;
    uint32_t end;
    uint32_t interval;
    std::vector<std::tuple<uint32_t, Height, uint64_t>> chartReversed;
};

struct ThrottleState {
    struct BatchThrottler {
        Height h0;
        Height h1;
        size_t window;
        template <size_t w>
        BatchThrottler(const BatchreqThrottler<w>& bt)
            : h0(bt.h0())
            , h1(bt.h1())
            , window(w)
        {
        }
    };
    sc::duration delay;
    BatchThrottler batchreq;
    BatchThrottler blockreq;
    ThrottleState(const ThrottleQueue& t)
        : delay(t.reply_delay())
        , batchreq(t.headerreq)
        , blockreq(t.blockreq)
    {
    }
};

struct Peerinfo {
    Peeraddr endpoint;
    uint64_t id;
    bool initialized;
    PeerChain chainstate;
    SignedSnapshot::Priority theirSnapshotPriority;
    SignedSnapshot::Priority acknowledgedSnapshotPriority;
    uint32_t since;
    ThrottleState throttle;
};

struct ThrottledPeer {
    Peeraddr endpoint;
    uint64_t id;
    ThrottleState throttle;
};

struct Network {
    /* data */
};

struct PeerinfoConnections {
    const std::vector<api::Peerinfo>& v;
    static constexpr auto map = [](const Peerinfo& pi) -> auto& { return pi.endpoint; };
};

struct Round16Bit {
    Wart original;
};

struct Wallet {
    PrivKey pk;
};

struct Raw {
    std::string s;
};

struct IPCounter {
    std::vector<std::pair<IP, size_t>> vector;
};

using OffenseEntry = ::OffenseEntry;

struct DBSize {
    size_t dbSize;
};

struct NodeInfo : public DBSize {
};
}
