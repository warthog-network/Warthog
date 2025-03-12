#pragma once
#include "block/body/transaction_id.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hasher_sha256.hpp"
#include "defi/token/token.hpp"
#include <functional>
#include <variant>
class Headerchain;
struct RewardInternal {
    ValidAccountId toAccountId;
    Wart amount;
    NonzeroHeight height;
    uint16_t offset; // id of payout in block
    AddressView toAddress { nullptr };
    Hash hash() const;
    RewardInternal(ValidAccountId toAccountId, Wart amount, NonzeroHeight height,
        uint16_t offset)
        : toAccountId(toAccountId)
        , amount(amount)
        , height(height)
        , offset(offset)
    {
    }
};
class VerifiedTransfer;
class TxIdVerifier;
struct TransferInternal {
    ValidAccountId fromAccountId;
    ValidAccountId toAccountId;
    Wart amount;
    PinNonce pinNonce;
    CompactUInt compactFee;
    AddressView fromAddress;
    AddressView toAddress;
    RecoverableSignature signature;
    [[nodiscard]] VerifiedTransfer verify(const Headerchain&, NonzeroHeight, const std::function<bool(TransactionId)>&) const;
};

class VerifiedTransfer {
    friend struct TransferInternal;
    VerifiedTransfer(const TransferInternal&, PinHeight pinHeight, HashView pinHash, const std::function<bool(TransactionId)>&);
    Address recover_address() const
    {
        return ti.signature.recover_pubkey(hash).address();
    }
    bool valid_signature() const;

public:
    const TransferInternal& ti;
    const VerifiedTransactionId id;
    const Hash hash;
};

class VerifiedTokenTransfer;
struct TokenTransferInternal {
    ValidAccountId fromAccountId;
    ValidAccountId toAccountId;
    Funds_uint64 amount;
    PinNonce pinNonce;
    CompactUInt compactFee;
    AddressView fromAddress { nullptr };
    AddressView toAddress { nullptr };
    RecoverableSignature signature;
    [[nodiscard]] VerifiedTokenTransfer verify(const Headerchain&, NonzeroHeight, HashView tokenHash) const;
    TokenTransferInternal(ValidAccountId from, CompactUInt compactFee, ValidAccountId to,
        Funds_uint64 amount, PinNonce pinNonce, View<65> signdata)
        : fromAccountId(from)
        , toAccountId(to)
        , amount(amount)
        , pinNonce(pinNonce)
        , compactFee(compactFee)
        , signature(signdata)
    {
    }
};

class VerifiedTokenTransfer {
    friend struct TokenTransferInternal;
    VerifiedTokenTransfer(const TokenTransferInternal&, PinHeight pinHeight, HashView pinHash, HashView tokenHash);
    Address recover_address() const
    {
        return ti.signature.recover_pubkey(hash).address();
    }
    bool valid_signature() const;

public:
    const TokenTransferInternal& ti;
    const TransactionId id;
    const Hash hash;
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
struct TransferData {
    static TransferData parse(Reader& r);
    constexpr static uint8_t indicator = 1;
    constexpr static uint8_t bytesize = 8 + 2 + 8 + 8 + 8; // without indicator
    AccountId fromAccountId;
    CompactUInt compactFee;
    AccountId toAccountId;
    Funds_uint64 amount;
    PinNonce pinNonce;
    void write(Writer& w) const;
};
struct RewardData {
    static RewardData parse(Reader& r);
    constexpr static uint8_t indicator = 2;
    constexpr static uint8_t bytesize = 8 + 8; // without indicator
    AccountId toAccountId;
    Funds_uint64 miningReward;
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

using Data = std::variant<TransferData, RewardData, TokenCreationData, TokenTransferData>;
struct Entry {
    Entry(const RewardInternal& p);
    Entry(const VerifiedTransfer& p);
    Entry(const VerifiedTokenTransfer& p, TokenId);
    Entry(const VerifiedTokenCreation& p);
    Hash hash;
    std::vector<uint8_t> data;
};
[[nodiscard]] Data parse_throw(std::vector<uint8_t>);
std::vector<uint8_t> serialize(const Data&);
}
