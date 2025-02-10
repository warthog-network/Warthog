#pragma once

#include "accountid_or_address.hpp"
#include "block/body/primitives.hpp"
#include "block/chain/history/index.hpp"
#include "block/chain/signed_snapshot.hpp"
#include "block/chain/worksum.hpp"
#include "block/header/difficulty_declaration.hpp"
#include "block/header/header.hpp"
#include "communication/mining_task.hpp"
#include "crypto/address.hpp"
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
class AccountCache;
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
    Funds amount;
};
struct TransferTransaction {
    Hash txhash;
    Address toAddress;
    uint32_t confirmations;
    Height height { 0 };
    uint32_t timestamp = 0;
    Funds amount;
    Address fromAddress;
    Funds fee;
    NonceId nonceId;
    PinHeight pinHeight { PinHeight::undef() };
};
struct AddressWithId {
    Address address;
    AccountId accountId;
};
struct Balance {
    std::optional<AddressWithId> address;
    Funds balance;
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
    Funds transferred;
    Funds totalTxFee;
    Funds blockReward;
};

struct Block {
    static constexpr const char eventName[] = "blockAppend";
    struct Transfer {
        Address fromAddress;
        Funds fee;
        NonceId nonceId;
        PinHeight pinHeight;
        Hash txhash;
        Address toAddress;
        Funds amount;
    };
    struct Reward {
        Hash txhash;
        Address toAddress;
        Funds amount;
    };
    Header header;
    NonzeroHeight height;
    uint32_t confirmations = 0;

private:
    std::optional<Reward> _reward; // optional because account's history is also returned in block structure
public:
    std::vector<Transfer> transfers;
    void push_history(const Hash& txid,
        const std::vector<uint8_t>& data, chainserver::AccountCache& cache,
        PinFloor pinFloor);

    Block(Header header,
        NonzeroHeight height, uint32_t confirmations,
        std::optional<Reward> reward = {}, std::vector<Transfer> transfers = {})
        : header(header)
        , height(height)
        , confirmations(confirmations)
        , _reward(std::move(reward))
        , transfers(std::move(transfers))
    {
    }
    void set_reward(Reward r);
    auto& reward() const { return _reward; }
};

struct AddressCount {
    Address address;
    int64_t count;
};

struct AccountHistory {
    Funds balance;
    HistoryId fromId;
    std::vector<api::Block> blocks_reversed;
};
struct TransactionsByBlocks {
    size_t count { 0 };
    HistoryId fromId;
    std::vector<api::Block> blocks_reversed;
};
struct Richlist {
    std::vector<std::pair<Address, Funds>> entries;
};
struct MempoolEntry : public TransferTxExchangeMessage {
    Hash txHash;
};
struct MempoolEntries {
    std::vector<MempoolEntry> entries;
};
struct OffenseHistory {
    std::vector<Hash> hashes;
    std::vector<TransferTxExchangeMessage> entries;
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
    Funds original;
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
