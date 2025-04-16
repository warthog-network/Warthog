#pragma once
#include "block/body/transaction_id.hpp"
#include "block/chain/history/index.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"
#include "defi/token/token.hpp"
#include "defi/uint64/price.hpp"
#include <functional>
#include <variant>
class Headerchain;

struct SwapInternal {
    HistoryId oId;
    TransactionId txid;
    Funds_uint64 base;
    Wart quote;
};

struct BuySwapInternal : public SwapInternal {
};
struct SellSwapInternal : public SwapInternal {
};

struct RewardInternal {
    ValidAccountId toAccountId;
    Wart amount;
    NonzeroHeight height;
    AddressView toAddress;
    Hash hash() const;
};

struct VerifiedHash : public Hash {
    VerifiedHash(Hash h, const RecoverableSignature& s, AddressView a)
        : Hash(h)
    {
        if (s.recover_pubkey(h).address() != a)
            throw Error(ECORRUPTEDSIG);
    }
};

struct IdAddressView {
    ValidAccountId id;
    AddressView address;
};

struct TransactionVerifier;
struct SignerData {
    friend struct TransactionVerifier;
    SignerData(ValidAccountId id, AddressView address, RecoverableSignature signature, PinNonce pinNonce, CompactUInt compactFee)
        : origin({ id, address })
        , signature(signature)
        , pinNonce(pinNonce)
        , compactFee(compactFee)
    {
    }
    IdAddressView origin;
    RecoverableSignature signature;
    PinNonce pinNonce;
    CompactUInt compactFee;

private:
    VerifiedHash verify_hash(Hash h) const
    {
        return { h, signature, origin.address };
    }
};

struct VerifiedTransaction {
    VerifiedHash hash;
    VerifiedTransactionId txid;

private:
    friend struct TransactionVerifier;
    VerifiedTransaction(VerifiedHash hash, VerifiedTransactionId txid)
        : hash(hash)
        , txid(txid)
    {
    }
};
struct TransactionVerifier {
    using validator_t = std::function<bool(TransactionId)>;
    const Headerchain& hc;
    NonzeroHeight h;
    validator_t validator;
    PinFloor pinFloor;
    TransactionVerifier(const Headerchain& hc, NonzeroHeight h, validator_t validator)
        : hc(hc)
        , h(h)
        , validator(std::move(validator))
        , pinFloor(h.pin_floor())
    {
    }

    struct PinInfo {
        PinHeight height;
        Hash hash;
    };

protected:
    PinInfo pin_info(PinNonce pinNonce) const;

public:
    template <typename... HashArgs>
    VerifiedTransaction verify(const SignerData& origin, HashArgs&&... hashArgs) const;
};

class TxIdVerifier;

struct TransferInternalWithoutAmount : public SignerData {
    IdAddressView to;
};

struct OrderInternal;
struct VerifiedOrder : public VerifiedTransaction {
    VerifiedOrder(const OrderInternal& o, const TransactionVerifier&, HashView tokenHash);

public:
    const OrderInternal& order;
};
struct OrderInternal : public SignerData {
    Price_uint64 limit;
    TokenFunds amount;
    bool buy;
    [[nodiscard]] VerifiedOrder verify(const TransactionVerifier& tv, HashView tokenHash) const
    {
        return { *this, tv, tokenHash };
    }
};

struct CancelationInternal;
struct VerifiedCancelation : public VerifiedTransaction {
    VerifiedCancelation(const CancelationInternal&, const TransactionVerifier&, HashView tokenHash);
    const CancelationInternal& cancelation;
};

struct CancelationInternal : public SignerData {
    TokenId tokenId;
    TransactionId cancelTxid;
    [[nodiscard]] VerifiedCancelation verify(const TransactionVerifier& tv, HashView tokenHash) const
    {
        return { *this, tv, tokenHash };
    }
};
struct LiquidityAddInternal : public SignerData {
};

struct LiquidityRemoveInternal : public SignerData {
};

struct WartTransferInternal;
class VerifiedWartTransfer : public VerifiedTransaction {
    friend struct WartTransferInternal;
    VerifiedWartTransfer(const WartTransferInternal&, const TransactionVerifier&); // Wart transfer

public:
    const WartTransferInternal& ti;
};

struct WartTransferInternal : public TransferInternalWithoutAmount {
    Wart amount;
    WartTransferInternal(TransferInternalWithoutAmount t, Wart amount)
        : TransferInternalWithoutAmount(std::move(t))
        , amount(amount)
    {
    }
    using TransferInternalWithoutAmount::TransferInternalWithoutAmount;
    [[nodiscard]] VerifiedWartTransfer verify(const TransactionVerifier& tv) const
    {
        return { *this, tv };
    }
};

struct TokenTransferInternal;
class VerifiedTokenTransfer : public VerifiedTransaction {
    friend struct TokenTransferInternal;
    VerifiedTokenTransfer(const TokenTransferInternal&, const TransactionVerifier&, HashView tokenHash);

public:
    const TokenTransferInternal& ti;
};
struct TokenTransferInternal : public TransferInternalWithoutAmount {
    Funds_uint64 amount;
    TokenTransferInternal(TransferInternalWithoutAmount t, Funds_uint64 amount)
        : TransferInternalWithoutAmount(std::move(t))
        , amount(amount)
    {
    }
    [[nodiscard]] VerifiedTokenTransfer verify(const TransactionVerifier& tv, HashView tokenHash) const
    {
        return { *this, tv, tokenHash };
    }
};

struct TokenCreationInternal;
struct VerifiedTokenCreation : public VerifiedTransaction {
    friend struct TokenCreationInternal;
    VerifiedTokenCreation(const TokenCreationInternal&, const TransactionVerifier&);
    const TokenCreationInternal& tci;
};

struct TokenCreationInternal : public SignerData {
    ValidAccountId creatorAccountId;
    PinNonce pinNonce;
    TokenName tokenName;
    CompactUInt compactFee;
    RecoverableSignature signature;
    AddressView creatorAddress;
    [[nodiscard]] VerifiedTokenCreation verify(const TransactionVerifier& tv) const
    {
        return { *this, tv };
    }
};

namespace history {
struct SwapHist : public SwapInternal {
    Hash hash;

protected:
    SwapHist(SwapInternal si, bool buyBase, Height h);
};

struct BuySwapHist : public SwapHist {
    BuySwapHist(BuySwapInternal s, Height h)
        : SwapHist(s, true, h)
    {
    }
};

struct SellSwapHist : public SwapHist {
    SellSwapHist(SellSwapInternal s, Height h)
        : SwapHist(s, false, h)
    {
    }
};

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

using Data = std::variant<WartTransferData, RewardData, TokenCreationData, TokenTransferData, OrderData, CancelationData, BuySwapData, SellSwapData>;

struct Entry {
    Entry(const RewardInternal& p);
    Entry(const VerifiedWartTransfer& p);
    Entry(const VerifiedTokenTransfer& p, TokenId);
    Entry(const VerifiedOrder& p);
    Entry(const VerifiedCancelation& p);
    Entry(const VerifiedTokenCreation& p, TokenId);
    Entry(const BuySwapHist& p);
    Entry(const SellSwapHist& p);
    Hash hash;
    std::vector<uint8_t> data;
};
[[nodiscard]] Data parse_throw(std::vector<uint8_t>);
std::vector<uint8_t> serialize(const Data&);
}
