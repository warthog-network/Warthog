#pragma once
#include "block/body/account_id.hpp"
#include "block/body/nonce.hpp"
#include "block/chain/height.hpp"
#include "general/compact_uint.hpp"
#include "general/view.hpp"
#include <array>
#include <cstdint>
class Height;
class PinHeight;
class Reader;

struct TransactionId {
    struct Generator {
        AccountId accountId;
        PinHeight pinHeight;
        NonceId nonceId;
    };
    TransactionId(Generator g)
        : TransactionId(g.accountId, g.pinHeight, g.nonceId)
    {
    }
    TransactionId(AccountId accountId, PinHeight pinHeight, NonceId nonceId)
        : accountId(accountId)
        , pinHeight(pinHeight)
        , nonceId(nonceId) { };
    constexpr static size_t bytesize = 16;
    static consteval size_t byte_size() { return bytesize; }

    TransactionId(Reader& r);
    std::string hex_string() const;
    friend Writer& operator<<(Writer&, const TransactionId&);
    auto operator<=>(const TransactionId& rhs) const = default;
    auto operator<=>(AccountId aid) const { return accountId <=> aid; }

    AccountId accountId;
    PinHeight pinHeight;
    NonceId nonceId;
};

struct VerifiedTransactionId : public TransactionId {
    VerifiedTransactionId(TransactionId txid, auto txIdValidator)
        : TransactionId(txid)
    {
        if (!txIdValidator(txid))
            throw Error(ENONCE);
    }
};

struct TxidWithFee {
    TransactionId txid;
    CompactUInt fee;

    TxidWithFee(TransactionId txid, CompactUInt fee)
        : txid(std::move(txid))
        , fee(std::move(fee))
    {
    }
    static consteval size_t byte_size() { return decltype(txid)::byte_size() + decltype(fee)::byte_size(); }
    friend Writer& operator<<(Writer&, const TxidWithFee&);

    TxidWithFee(Reader& r);
};
