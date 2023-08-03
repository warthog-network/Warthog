#pragma once
#include "general/compact_uint.hpp"
#include "block/body/primitives.hpp"
#include "block/body/transaction_id.hpp"
#include "block/chain/pin.hpp"
#include "crypto/hash.hpp"
#include "general/funds.hpp"
#include "mempool/order_key.hpp"
class TransferTxExchangeMessageView;

namespace mempool {
struct EntryValue;
using Entry = std::pair<TransactionId,EntryValue>;
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
    NonceReserved noncep2;
    CompactUInt fee;
    Address toAddr;
    Funds amount;
    RecoverableSignature signature;
    Hash hash;
    Height transactionHeight; // when was the account first registered
};
}
