#pragma once
#include "block/body/transaction_id.hpp"
#include "block/chain/height.hpp"
#include "crypto/address.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"

class Address;
class HashView;
struct TransferView;

class PaymentCreateMessage;

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
    TransferTxExchangeMessage(TransferView, PinHeight, AddressView toAddr);

    friend Writer& operator<<(Writer&, TransferTxExchangeMessage);
    [[nodiscard]] TxHash txhash(HashView pinHash) const;
    [[nodiscard]] Address from_address(HashView txHash) const;
    [[nodiscard]] Funds spend() const { return fee() + amount; }
    Funds fee() const { return compactFee.uncompact(); }
    AccountId from_id() const { return txid.accountId; }
    PinHeight pin_height() const { return txid.pinHeight; }
    NonceId nonce_id() const { return txid.nonceId; }

    TransactionId txid;
    NonceReserved reserved;
    CompactUInt compactFee;
    Address toAddr;
    Funds amount;
    RecoverableSignature signature;
};
