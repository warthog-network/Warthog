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
#include "db/offense_entry.hpp"
#include "eventloop/peer_chain.hpp"
#include "eventloop/types/conndata.hpp"
#include "general/funds.hpp"
#include "general/tcp_util.hpp"
#include "height_or_hash.hpp"
#include <variant>
#include <vector>
namespace chainserver {
class AccountCache;
}

namespace API {
using sc = std::chrono::steady_clock;
struct ChainHead {
    std::optional<SignedSnapshot> signedSnapshot;
    Worksum worksum;
    Target nextTarget;
    Hash hash;
    Height height;
    Hash pinHash;
    PinHeight pinHeight;
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
struct Balance {
    std::optional<Address> address;
    AccountId accountId;
    Funds balance;
};

struct Rollback {
    static constexpr const char WEBSOCKET_EVENT[] = "Rollback";
    Height length;
};

struct Block {
    static constexpr const char WEBSOCKET_EVENT[] = "Block";
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
    std::vector<Transfer> transfers;
    std::vector<Reward> rewards;
    void push_history(const Hash& txid,
        const std::vector<uint8_t>& data, chainserver::AccountCache& cache,
        PinFloor pinFloor);

    Block(Header header,
        NonzeroHeight height, uint32_t confirmations)
        : header(header)
        , height(height)
        , confirmations(confirmations)
    {
    }
};
struct AccountHistory {
    Funds balance;
    HistoryId fromId;
    std::vector<API::Block> blocks_reversed;
};
struct TransactionsByBlocks {
    size_t count { 0 };
    HistoryId fromId;
    std::vector<API::Block> blocks_reversed;
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

struct HashrateChart {
    HashrateChartRequest range;
    std::vector<double> chart;
};

struct ThrottleState {
    struct BatchThrottler {
        Height h1;
        Height h2;
        size_t window;
        template <size_t w>
        BatchThrottler(const BatchreqThrottler<w>& bt)
            : h1(bt.h1())
            , h2(bt.h2())
            , window(w)
        {
        }
    };
    sc::duration delay;
    BatchThrottler batchreq;
    BatchThrottler blockreq;
    ThrottleState(const Throttled& t)
        : delay(t.reply_delay())
        , batchreq(t.batchreq)
        , blockreq(t.blockreq)
    {
    }
};

struct Peerinfo {
    EndpointAddress endpoint;
    uint64_t id;
    bool initialized;
    PeerChain chainstate;
    SignedSnapshot::Priority theirSnapshotPriority;
    SignedSnapshot::Priority acknowledgedSnapshotPriority;
    uint32_t since;
    ThrottleState throttle;
};

struct ThrottledPeer{
    EndpointAddress endpoint;
    uint64_t id;
    ThrottleState throttle;
};

struct Network {
    /* data */
};

struct PeerinfoConnections {
    const std::vector<API::Peerinfo>& v;
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

using OffenseEntry = ::OffenseEntry;

}
