#pragma once
#include "block/body/transaction_id.hpp"
#include "block/chain/height.hpp"
#include "crypto/address.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"

class Address;
class HashView;
struct WartTransferView;

class WartTransferCreate;
class TokenTransferCreate;

namespace mempool {
namespace entry {
    struct Value;
    struct Shared;
    struct WartTransfer;
}
}

class WartTransferMessage {
public:
    // layout:
    static constexpr size_t bytesize = 16 + 3 + 2 + 20 + 8 + 65;
    static constexpr size_t byte_size() { return bytesize; }
    WartTransferMessage(ReaderCheck<bytesize> r);
    WartTransferMessage(AccountId fromId, const WartTransferCreate& pcm);
    WartTransferMessage(const TransactionId& txid,
        const mempool::entry::Shared& s,
        const mempool::entry::WartTransfer& t);
    WartTransferMessage(WartTransferView, PinHeight, AddressView toAddr);

    friend Writer& operator<<(Writer&, WartTransferMessage);
    [[nodiscard]] TxHash txhash(HashView pinHash) const;
    [[nodiscard]] Address from_address(HashView txHash) const;
    [[nodiscard]] Funds_uint64 spend_throw() const { return Funds_uint64::sum_throw(fee(), amount); }
    Wart fee() const { return compactFee.uncompact(); }
    AccountId from_id() const { return txid.accountId; }
    PinHeight pin_height() const { return txid.pinHeight; }
    NonceId nonce_id() const { return txid.nonceId; }

    TransactionId txid;
    NonceReserved reserved;
    CompactUInt compactFee;
    Address toAddr;
    Wart amount;
    RecoverableSignature signature;
};

class TokenTransferMessage { // for defi we include the token id
public:
    // layout:
    static constexpr size_t bytesize = 16 + 3 + 2 + 32 + 20 + 8 + 65;
    static constexpr size_t byte_size() { return bytesize; }
    TokenTransferMessage(ReaderCheck<bytesize> r);
    TokenTransferMessage(AccountId fromId, const TokenTransferCreate& pcm);
    TokenTransferMessage(const TransactionId& txid, const mempool::entry::Value&);
    TokenTransferMessage(WartTransferView, Hash tokenHash, PinHeight, AddressView toAddr);

    friend Writer& operator<<(Writer&, TokenTransferMessage);
    [[nodiscard]] TxHash txhash(HashView pinHash) const;
    [[nodiscard]] Address from_address(HashView txHash) const;
    [[nodiscard]] Funds_uint64 spend_throw() const { return Funds_uint64::sum_throw(fee(), amount); }
    Funds_uint64 fee() const { return compactFee.uncompact(); }
    AccountId from_id() const { return txid.accountId; }
    PinHeight pin_height() const { return txid.pinHeight; }
    NonceId nonce_id() const { return txid.nonceId; }

    TransactionId txid;
    NonceReserved reserved;
    CompactUInt compactFee;
    TokenHash tokenHash;
    Address toAddr;
    Funds_uint64 amount;
    RecoverableSignature signature;
};

class CreateOrderMessage {
};
class CancelOrderMessage {
};
class AddLiquidityMessage {
};
class RemoveLiquidityMessage {
};
