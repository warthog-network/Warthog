#pragma once
#include "block/body/primitives.hpp"
#include "block/body/transaction_id.hpp"
#include "block/chain/pin.hpp"
#include "crypto/hash.hpp"
#include "general/compact_uint.hpp"
#include "general/funds.hpp"
#include "mempool/order_key.hpp"
class TransferTxExchangeMessageView;

namespace mempool {
struct EntryValue;
struct EntryValue {
    EntryValue(NonceReserved noncep2, CompactUInt fee, Address toAddr, Funds amount, RecoverableSignature signature, Hash hash, Height transactionHeight)
        : noncep2(noncep2)
        , fee(fee)
        , toAddr(toAddr)
        , amount(amount)
        , signature(signature)
        , hash(hash)
        , transactionHeight(transactionHeight)
    {
    }
    [[nodiscard]] auto spend_assert() const { return Funds::sum_assert(fee.uncompact(), amount); }
    NonceReserved noncep2;
    CompactUInt fee;
    Address toAddr;
    Funds amount;
    RecoverableSignature signature;
    Hash hash;
    Height transactionHeight; // when was the account first registered
};

class Entry {
    TransactionId txid;
    EntryValue entryValue;

public:
    Entry(TransactionId txid, EntryValue entryValue)
        : txid(std::move(txid))
        , entryValue(std::move(entryValue))
    {
    }
    Entry(std::pair<const TransactionId, EntryValue>& p)
        : Entry(p.first, p.second)
    {
    }
    auto& entry_value() const { return entryValue; }
    auto& transaction_id() const { return txid; }
    auto transaction_height() const { return entryValue.transactionHeight; }
    auto& to_address() const { return entryValue.toAddr; }
    auto fee() const { return entryValue.fee; }
    auto amount() const { return entryValue.amount; }
    auto nonce_id() const { return txid.nonceId; }
    auto pin_height() const { return txid.pinHeight; }
    auto tx_hash() const { return entryValue.hash.hex_string(); }
    auto from_address() const { return entryValue.signature.recover_pubkey(entryValue.hash).address(); }
};
}
