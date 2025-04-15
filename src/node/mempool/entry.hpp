#pragma once
#include "block/body/primitives.hpp"
#include "block/body/transaction_id.hpp"
#include "crypto/hash.hpp"
#include "defi/uint64/price.hpp"
#include "general/compact_uint.hpp"
#include "general/funds.hpp"
#include "tools/variant.hpp"
class TransferTxExchangeMessageView;

namespace mempool {
namespace entry {
    struct WartTransfer {
        WartTransfer(Address toAddr, Wart amount)
            : toAddr(toAddr)
            , amount(amount)
        {
        }
        Address toAddr;
        Wart amount;
    };

    struct TokenTransfer {
        TokenTransfer(TokenHash tokenHash, Address toAddr, Funds_uint64 amount, TokenPrecision precision)
            : tokenHash(tokenHash)
            , toAddr(toAddr)
            , amount(amount)
            , precision(precision)
        {
        }
        TokenHash tokenHash;
        Address toAddr;
        Funds_uint64 amount;
        TokenPrecision precision;
    };

    struct CreateOrder {
        CreateOrder(TokenHash tokenHash, Funds_uint64 amount, Price_uint64 limit, bool buy)
            : tokenHash(std::move(tokenHash))
            , amount(amount)
            , limit(limit)
            , buy(buy)
        {
        }

        TokenHash tokenHash;
        Funds_uint64 amount;
        Price_uint64 limit;
        bool buy;
    };
    struct CancelOrder {
        TokenId tokenId;
        TransactionId cancelTxid;
    };
    struct AddLiquidity {
    };
    struct RemoveLiquidity {
    };

    struct Value {
        TransactionHeight transactionHeight;
        NonceReserved noncep2;
        CompactUInt fee;
        Hash txHash;
        RecoverableSignature signature;
        wrt::variant<WartTransfer, TokenTransfer, CreateOrder,CancelOrder, AddLiquidity, RemoveLiquidity> variant;
    };

}

class Entry {
    using EntryValue = entry::Value;
    TransactionId txid;
    // std::variant
    EntryValue data;

public:
    Entry(TransactionId txid, EntryValue entryValue)
        : txid(std::move(txid))
        , data(std::move(entryValue))
    {
    }
    Entry(std::pair<const TransactionId, EntryValue>& p)
        : Entry(p.first, p.second)
    {
    }
    auto& entry_value() const { return data; }
    auto& transaction_id() const { return txid; }
    auto transaction_height() const { return data.transactionHeight; }
    auto fee() const { return data.fee; }
    // auto amount() const { return data.amount; }
    // FundsDecimal amount_decimal() const { return FundsDecimal { data.amount.value(), data.precision.value() }; }
    auto nonce_id() const { return txid.nonceId; }
    auto pin_height() const { return txid.pinHeight; }
    auto tx_hash() const { return data.txHash.hex_string(); }
    auto from_address() const { return data.signature.recover_pubkey(data.txHash).address(); }
};
}
