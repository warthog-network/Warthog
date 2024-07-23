#pragma once
#include "block/body/transaction_id.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hasher_sha256.hpp"
#include <variant>
class Headerchain;
struct RewardInternal {
    AccountId toAccountId;
    Funds amount;
    NonzeroHeight height;
    uint16_t offset; // id of payout in block
    AddressView toAddress { nullptr };
    Hash hash() const;
    RewardInternal(AccountId toAccountId, Funds amount, NonzeroHeight height,
        uint16_t offset)
        : toAccountId(toAccountId)
        , amount(amount)
        , height(height)
        , offset(offset)
    {
    }
};
class VerifiedTransfer;
struct TransferInternal {
    AccountId fromAccountId;
    AccountId toAccountId;
    Funds amount;
    PinNonce pinNonce;
    CompactUInt compactFee;
    AddressView fromAddress { nullptr };
    AddressView toAddress { nullptr };
    RecoverableSignature signature;
    VerifiedTransfer verify(const Headerchain&, NonzeroHeight) const;
    TransferInternal(AccountId from, CompactUInt compactFee, AccountId to,
        Funds amount, PinNonce pinNonce, View<65> signdata)
        : fromAccountId(from)
        , toAccountId(to)
        , amount(amount)
        , pinNonce(pinNonce)
        , compactFee(compactFee)
        , signature(signdata)
    {
    }
};

class VerifiedTransfer {
    friend struct TransferInternal;
    VerifiedTransfer(const TransferInternal&, PinHeight pinHeight, HashView pinHash);
    Address recover_address() const
    {
        return ti.signature.recover_pubkey(hash).address();
    }
    bool valid_signature() const;

public:
    const TransferInternal& ti;
    const TransactionId id;
    const Hash hash;
};

namespace history {
struct TransferData {
    static TransferData parse(Reader& r);
    constexpr static uint8_t indicator = 1;
    constexpr static uint8_t bytesize = 8 + 2 + 8 + 8 + 8; // without indicator
    AccountId fromAccountId;
    CompactUInt compactFee;
    AccountId toAccountId;
    Funds amount;
    PinNonce pinNonce;
    void write(Writer& w) const;
};
struct RewardData {
    static RewardData parse(Reader& r);
    constexpr static uint8_t indicator = 2;
    constexpr static uint8_t bytesize = 8 + 8; // without indicator
    AccountId toAccountId;
    Funds miningReward;
    void write(Writer& w) const;
};
using Data = std::variant<TransferData, RewardData>;
struct Entry {
    Entry(const RewardInternal& p);
    Entry(const VerifiedTransfer& p);
    Hash hash;
    std::vector<uint8_t> data;
};
[[nodiscard]] Data parse_throw(std::vector<uint8_t>);
std::vector<uint8_t> serialize(const Data&);
}
