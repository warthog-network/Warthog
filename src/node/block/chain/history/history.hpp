#pragma once
#include "block/body/transaction_id.hpp"
#include "block/chain/history/index.hpp"
#include "chainserver/state/block_apply/types.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"
#include "defi/token/token.hpp"
#include "defi/uint64/price.hpp"
#include <variant>
class Headerchain;

class TxIdVerifier;

namespace history {
struct WartTransferData {
    static WartTransferData parse(Reader& r);
    constexpr static uint8_t indicator = 1;
    constexpr static uint8_t bytesize = 8 + 2 + 8 + 8 + 8; // without indicator
    AccountId fromAccountId;
    CompactUInt compactFee;
    AccountId toAccountId;
    Wart amount;
    PinNonce pinNonce;
    void write(Writer& w) const;
};
struct RewardData {
    static RewardData parse(Reader& r);
    constexpr static uint8_t indicator = 2;
    constexpr static uint8_t bytesize = 8 + 8; // without indicator
    AccountId toAccountId;
    Wart miningReward;
    void write(Writer& w) const;
};

struct TokenCreationData {
    static TokenCreationData parse(Reader& r);
    constexpr static uint8_t indicator = 3;
    constexpr static uint8_t bytesize = 8 + 8 + 6 + 2 + 4; // without indicator
    AccountId creatorAccountId; // 8 bytes
    PinNonce pinNonce; // 8 bytes
    TokenName tokenName; // 6 bytes
    CompactUInt compactFee; // 2 bytes
    TokenId tokenId; // 4 bytes
    void write(Writer& w) const;
};

struct TokenTransferData {
    static TokenTransferData parse(Reader& r);
    constexpr static uint8_t indicator = 4;
    constexpr static uint8_t bytesize = 4 + 8 + 2 + 8 + 8 + 8; // without indicator
    TokenId tokenId;
    AccountId fromAccountId;
    CompactUInt compactFee;
    AccountId toAccountId;
    Funds_uint64 amount;
    PinNonce pinNonce;
    void write(Writer& w) const;
};

struct OrderData {
    TokenId tokenId;
    bool buy;
    AccountId accountId;
    CompactUInt compactFee;
    Price_uint64 limit;
    Funds_uint64 amount;
    PinNonce pinNonce;
    constexpr static uint8_t indicator = 5;
    constexpr static uint8_t bytesize = 4 + 1 + 8 + 2 + 8 + 8 + 8; // without indicator
    void write(Writer& w) const;
    static OrderData parse(Reader& r);
};

struct CancelationData {
    TokenId tokenId;
    TransactionId cancelTxid;
    AccountId accountId;
    CompactUInt compactFee;
    constexpr static uint8_t indicator = 6;
    constexpr static uint8_t bytesize = 4 + 16 + 8 + 2; // without indicator
    void write(Writer& w) const;
    static CancelationData parse(Reader& r);
};

struct SwapData {
    SwapData(const SwapInternal& si)
        : oId(si.oId)
        , accId(si.txid.accountId)
        , base(si.base)
        , quote(si.quote)
    {
    }

    SwapData(HistoryId oId, AccountId accId, Funds_uint64 base, Wart quote)
        : oId(oId)
        , accId(accId)
        , base(base)
        , quote(quote)
    {
    }

    HistoryId oId;
    AccountId accId;
    Funds_uint64 base;
    Wart quote;
    constexpr static uint8_t bytesize = 8 + 8 + 8 + 8; // without indicator
    static SwapData parse(Reader&);
    void write(Writer& w) const;
};
struct BuySwapData : public SwapData {
    constexpr static uint8_t indicator = 7;
    explicit BuySwapData(SwapData sd)
        : SwapData(std::move(sd))
    {
    }
    static BuySwapData parse(Reader& r)
    {
        return BuySwapData { SwapData::parse(r) };
    }
};
struct SellSwapData : public SwapData {
    constexpr static uint8_t indicator = 8;
    explicit SellSwapData(SwapData sd)
        : SwapData(std::move(sd))
    {
    }
    static SellSwapData parse(Reader& r)
    {
        return SellSwapData { SwapData::parse(r) };
    }
};

using data_t = std::variant<WartTransferData, RewardData, TokenCreationData, TokenTransferData, OrderData, CancelationData, BuySwapData, SellSwapData>;

struct Data : public data_t {
    using data_t::data_t;
    Data(data_t d)
        : data_t(std::move(d))
    {
    }
    static Data parse_throw(std::vector<uint8_t>);
};

struct Entry {
    Entry(const RewardInternal& p);
    Entry(const VerifiedWartTransfer& p);
    Entry(const VerifiedTokenTransfer& p, TokenId);
    Entry(const VerifiedOrder& p);
    Entry(const VerifiedCancelation& p);
    Entry(const VerifiedTokenCreation& p, TokenId);
    Entry(const ProcessedBuySwap& p);
    Entry(const ProcessedSellSwap& p);
    Hash hash;
    std::vector<uint8_t> data;
};
}
