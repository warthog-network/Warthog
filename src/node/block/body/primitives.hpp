#pragma once
#include "block/body/transaction_id.hpp"
#include "block/chain/height.hpp"
#include "crypto/address.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"
#include "defi/token/id.hpp"

class Address;
class HashView;
struct WartTransferView;

class PaymentCreateMessage;
class TokenPaymentCreateMessage;

namespace mempool {
struct EntryValue;
}

class TransferTxExchangeMessage {
public:
    // layout:
    static constexpr size_t bytesize = 16 + 3 + 2 + 20 + 8 + 65;
    static constexpr size_t byte_size() { return bytesize; }
    TransferTxExchangeMessage(ReaderCheck<bytesize> r);
    TransferTxExchangeMessage(AccountId fromId, const PaymentCreateMessage& pcm);
    TransferTxExchangeMessage(const TransactionId& txid, const mempool::EntryValue&);
    TransferTxExchangeMessage(WartTransferView, PinHeight, AddressView toAddr);

    friend Writer& operator<<(Writer&, TransferTxExchangeMessage);
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

class TransferDefiMessage { // for defi we include the token id
public:
    // layout:
    static constexpr size_t bytesize = 16 + 3 + 2 + 32 + 20 + 8 + 65;
    static constexpr size_t byte_size() { return bytesize; }
    TransferDefiMessage(ReaderCheck<bytesize> r);
    TransferDefiMessage(AccountId fromId, const TokenPaymentCreateMessage& pcm);
    TransferDefiMessage(const TransactionId& txid, const mempool::EntryValue&);
    TransferDefiMessage(WartTransferView, Hash tokenHash, PinHeight, AddressView toAddr);

    friend Writer& operator<<(Writer&, TransferDefiMessage);
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
