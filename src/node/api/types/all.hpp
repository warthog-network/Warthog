#pragma once

#include "accountid_or_address.hpp"
#include "api/types/input.hpp"
#include "block/body/primitives.hpp"
#include "block/chain/history/index.hpp"
#include "block/chain/signed_snapshot.hpp"
#include "block/chain/worksum.hpp"
#include "block/header/difficulty_declaration.hpp"
#include "block/header/header.hpp"
#include "communication/mining_task.hpp"
#include "crypto/address.hpp"
#include "defi/token/token.hpp"
#include "defi/uint64/price.hpp"
#include "eventloop/peer_chain.hpp"
#include "eventloop/types/conndata.hpp"
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
    Hash pinHash;
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

struct RewardTransaction {
    Hash txhash;
    Address toAddress;
    uint32_t confirmations;
    Height height { 0 };
    uint32_t timestamp = 0;
    Wart amount;
};
struct TransferTransaction {
    Hash txhash;
    Address toAddress;
    uint32_t confirmations;
    Height height { 0 };
    uint32_t timestamp = 0;
    Wart amount;
    Address fromAddress;
    Wart fee;
    NonceId nonceId;
    PinHeight pinHeight { PinHeight::undef() };
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
    std::optional<AddressWithId> address;
    FundsDecimal balance;
    TokenBalance()
        : balance(FundsDecimal::zero())
    {
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

struct Block {
    static constexpr const char eventName[] = "blockAppend";
    struct Transfer {
        Address fromAddress;
        Wart fee;
        NonceId nonceId;
        PinHeight pinHeight;
        Hash txhash;
        Address toAddress;
        Wart amount;
    };
    struct TokenTransfer {
        TokenIdHashNamePrecision tokenInfo;
        Address fromAddress;
        Wart fee;
        NonceId nonceId;
        PinHeight pinHeight;
        Hash txhash;
        Address toAddress;
        FundsDecimal amount;
    };
    struct NewOrder {
        TokenIdHashNamePrecision tokenInfo;
        Wart fee;
        FundsDecimal amount;
        Price_uint64 limit;
        bool buy;
        TransactionId txid;
        Hash txhash;
        Address address;
    };
    struct Swap {
        TokenIdHashNamePrecision tokenInfo;
        Hash txhash;
        bool buy;
        Wart fillQuote;
        FundsDecimal fillBase;
    };
    struct Reward {
        Hash txhash;
        Address toAddress;
        Wart amount;
    };
    struct TokenCreation {
        TransactionId txid;
        Hash txhash;
        TokenName tokenName;
        FundsDecimal supply;
        TokenId tokenId;
        Wart fee;
    };
    struct Cancelation {
        struct OrderData {
            TokenId tid;
            Funds_uint64 total;
            Funds_uint64 filled;
            Price_uint64 limit;
            bool buy;
        };
        IdAddress origin;
        Wart fee;
        Hash txhash;
        std::optional<OrderData> order;
    };
    Header header;
    NonzeroHeight height;
    uint32_t confirmations = 0;

public:
    struct Actions {
        std::optional<Reward> reward;
        std::vector<Transfer> wartTransfers;
        std::vector<TokenTransfer> tokenTransfers;
        std::vector<TokenCreation> tokenCreations;
        std::vector<NewOrder> newOrders;
        std::vector<Swap> swaps;
        std::vector<Cancelation> cancelations;
    } actions;
    void push_history(const Hash& txid,
        const std::vector<uint8_t>& data, chainserver::DBCache& cache,
        PinFloor pinFloor);

    Block(Header header,
        NonzeroHeight height, uint32_t confirmations,
        Actions actions)
        : header(header)
        , height(height)
        , confirmations(confirmations)
        , actions(std::move(actions))
    {
    }
    void set_reward(Reward r);
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
    Hash txHash;
};
struct MempoolEntries {
    std::vector<MempoolEntry> entries;
};
struct OffenseHistory {
    std::vector<Hash> hashes;
    std::vector<WartTransferMessage> entries;
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
