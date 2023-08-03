#pragma once
#include "block/body/account_id.hpp"
#include "general/compact_uint.hpp"
#include "block/body/nonce.hpp"
#include "block/chain/height.hpp"
#include "general/reader.hpp"
#include "general/view.hpp"
#include <array>
#include <cstdint>
class Height;
class PinHeight;

struct TransactionId {
    TransactionId(AccountId accountId, PinHeight pinHeight, NonceId nonceId)
        : accountId(accountId)
        , pinHeight(pinHeight)
        , nonceId(nonceId) {};
    constexpr static size_t bytesize = 16;

    TransactionId(Reader& r);
    friend Writer& operator<<(Writer&, const TransactionId&);
    auto operator<=>(const TransactionId& rhs) const = default;

    AccountId accountId;
    PinHeight pinHeight;
    NonceId nonceId;
};

struct TxidWithFee {
    TransactionId txid;
    CompactUInt fee;
};
