#pragma once
#include "block/body/elements.hpp"
#include "block/body/transaction_id.hpp"
#include "block/chain/height.hpp"
#include "crypto/address.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"
#include "general/reader.hpp"
#include "tools/variant.hpp"

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
struct TokenTransfer;
}
}




class WartTransferMessageData {
    Address toAddr;
    Wart amount;
};

template <typename Data>
class TransactionMessage {
    TransactionId txid;
    NonceReserved reserved;
    CompactUInt compactFee;
    Data data;
    static constexpr size_t byte_size() { return 16 + 3 + 2 + Data::byte_size() + 65; }
    RecoverableSignature signature;
};

class WartTransferMessageDeprecated {
public:
    using WartTransferView = block::body::WartTransfer;
    // layout:
    static constexpr size_t bytesize = 16 + 3 + 2 + 20 + 8 + 65;
    static constexpr size_t byte_size() { return bytesize; }
    WartTransferMessageDeprecated(ReaderCheck<bytesize> r);
    WartTransferMessageDeprecated(
        const TransactionId& txid,
        const mempool::entry::Shared& s,
        const mempool::entry::WartTransfer& t);
    WartTransferMessageDeprecated(WartTransferView, PinHeight, AddressView toAddr);

    friend Writer& operator<<(Writer&, WartTransferMessageDeprecated);
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

class TokenTransferMessageDeprecated { // for defi we include the token id
public:
    using TokenTransferView = block::body::TokenTransfer;
    // layout:
    static constexpr size_t bytesize = 16 + 3 + 2 + 32 + 20 + 8 + 65;
    static constexpr size_t byte_size() { return bytesize; }
    TokenTransferMessageDeprecated(ReaderCheck<bytesize> r);
    TokenTransferMessageDeprecated(const TransactionId& txid, const mempool::entry::Value&);
    TokenTransferMessageDeprecated(const TransactionId& txid, const mempool::entry::Shared& s, const mempool::entry::TokenTransfer& v);
    TokenTransferMessageDeprecated(TokenTransferView, Hash tokenHash, PinHeight, AddressView toAddr);

    friend Writer& operator<<(Writer&, TokenTransferMessageDeprecated);
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
    TokenHash tokenHash;
    Address toAddr;
    Funds_uint64 amount;
    RecoverableSignature signature;
};

class CreateOrderMessageDeprecated {
    RecoverableSignature signature;
};
class CancelMessageDeprecated {
};
class AddLiquidityMessageDeprecated {
};
class RemoveLiquidityMessageDeprecated {
};

// using TransactionVariant = wrt::variant<WartTransferMessageDeprecated, TokenTransferMessageDeprecated, CreateOrderMessageDeprecated, CancelMessageDeprecated, AddLiquidityMessageDeprecated, RemoveLiquidityMessageDeprecated>;
using TransactionVariant = wrt::variant<WartTransferMessageDeprecated, TokenTransferMessageDeprecated>;
struct TransactionMessageDeprecated : public TransactionVariant {
    Wart fee() const
    {
        return visit([](auto& message) { return message.fee(); });
    }
};
