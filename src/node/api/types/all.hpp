#pragma once

#include "block/body/primitives.hpp"
#include "block/chain/signed_snapshot.hpp"
#include "block/chain/worksum.hpp"
#include "block/header/header.hpp"
#include "block/header/difficulty_declaration.hpp"
// #include 
#include "crypto/address.hpp"
#include "db/offense_entry.hpp"
#include "eventloop/peer_chain.hpp"
#include "general/funds.hpp"
#include "general/tcp_util.hpp"
#include <vector>
namespace chainserver {
class AccountCache;
}

namespace API {
struct Head {
    std::optional<SignedSnapshot> signedSnapshot;
    Worksum worksum;
    Target nextTarget;
    Hash hash;
    Height height;
    Hash pinHash;
    PinHeight pinHeight;
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
    AccountId accountId;
    Funds balance;
};
struct Block {
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
    Height height { 0 };
    uint32_t confirmations = 0;
    std::vector<Transfer> transfers;
    std::vector<Reward> rewards;
    void push_history(const Hash& txid,
        const std::vector<uint8_t>& data, chainserver::AccountCache& cache,
        PinFloor pinFloor);

    Block(Header header,
        Height height, uint32_t confirmations)
        : header(header)
        , height(height)
        , confirmations(confirmations)
    {
    }
};
struct History {
    Funds balance;
    uint64_t fromId;
    std::vector<API::Block> blocks_reversed;
};
struct Richlist{
    std::vector<std::pair<Address,Funds>> entries;
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
    uint64_t by100Blocks;
};
struct Peerinfo {
    IPv4 ip;
    bool initialized;
    PeerChain chainstate;
    SignedSnapshot::Priority theirSnapshotPriority;
    SignedSnapshot::Priority acknowledgedSnapshotPriority;
    uint32_t since;
    uint16_t port;
};
using OffenseEntry = ::OffenseEntry;
}
