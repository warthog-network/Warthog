#pragma once
#include "block/body/transaction_id.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"
#include "defi/token/token.hpp"
#include "defi/uint64/price.hpp"
#include <functional>
#include <variant>
class Headerchain;

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

struct SignerData : public IdAddressView {
    SignerData(ValidAccountId id, AddressView address, RecoverableSignature signature, PinNonce pinNonce)
        : IdAddressView({ id, address })
        , signature(signature)
        , pinNonce(pinNonce)
    {
    }

    RecoverableSignature signature;
    PinNonce pinNonce;
    VerifiedHash verify_hash(Hash h) const
    {
        return { h, signature, address };
    }
};

struct TransactionVerifier;
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

struct TransferInternalWithoutAmount {
    SignerData from;
    IdAddressView to;
    CompactUInt compactFee;
};

struct OrderInternal {
    SignerData signer;
    PriceRelative_uint64 limit;
    TokenFunds amount;
    bool buy;
};

struct CancelationInternal {
    SignerData signer;
    TransactionId txid;
};
struct LiquidityAddInternal {
    SignerData signer;
};

struct LiquidityRemoveInternal {
    SignerData signer;
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
        return VerifiedWartTransfer(*this, tv);
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

class VerifiedTokenCreation;
struct TokenCreationInternal {
    ValidAccountId creatorAccountId;
    PinNonce pinNonce;
    TokenName tokenName;
    CompactUInt compactFee;
    RecoverableSignature signature;
    AddressView creatorAddress;
    [[nodiscard]] VerifiedTokenCreation verify(const Headerchain&, NonzeroHeight, TokenId) const;
};

class VerifiedTokenCreation {
    friend struct TokenCreationInternal;
    VerifiedTokenCreation(const TokenCreationInternal&, PinHeight pinHeight, HashView pinHash, TokenId);
    Address recover_address() const
    {
        return tci.signature.recover_pubkey(hash).address();
    }

    bool valid_signature() const;

public:
    const TokenCreationInternal& tci;
    TransactionId id;
    Hash hash;
    TokenId tokenIndex;
};

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
    TokenId tokenIndex; // 4 bytes
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

using Data = std::variant<WartTransferData, RewardData, TokenCreationData, TokenTransferData>;
struct Entry {
    Entry(const RewardInternal& p);
    Entry(const VerifiedWartTransfer& p);
    Entry(const VerifiedTokenTransfer& p, TokenId);
    Entry(const VerifiedTokenCreation& p);
    Hash hash;
    std::vector<uint8_t> data;
};
[[nodiscard]] Data parse_throw(std::vector<uint8_t>);
std::vector<uint8_t> serialize(const Data&);
}
