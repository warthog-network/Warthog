#pragma once
#include "block/body/account_id.hpp"
#include "block/body/nonce.hpp"
#include "block/body/primitives.hpp"
#include "block/body/transaction_id.hpp"
#include "block/chain/height.hpp"
#include "defi/token/token.hpp"
#include "defi/uint64/price.hpp"
#include "general/compact_uint.hpp"
#include "general/funds.hpp"
#include "general/reader.hpp"
#include "general/view.hpp"
namespace block {
namespace body {
namespace view {
constexpr static size_t SIGLEN { 65 };
// constexpr static size_t AddressSize { 20 };
// constexpr static size_t TransferSize { 34 + SIGLEN };
// constexpr static size_t OrderSize { 30 + SIGLEN };
// constexpr static size_t CancelSize { 24 + SIGLEN };
// constexpr static size_t LiquidityAddSize { 34 + SIGLEN };
// constexpr static size_t LiquidityRemoveSize { 26 + SIGLEN };
// constexpr static size_t CancelationSize { 16 + SIGLEN }; // TODO
// constexpr static size_t TokenCreationSize { 8 + 8 + 5 + 2 + SIGLEN };

struct Reward : public View<16> {
private:
    auto funds_value() const
    {
        return readuint64(pos + 8);
    }

public:
    using View::View;

    AccountId account_id() const
    {
        return AccountId(readuint64(pos));
    }
    Wart amount_throw() const
    {
        return Wart::from_value_throw(funds_value());
    }
    Funds_uint64 amount_assert() const
    {
        auto f { Funds_uint64::from_value(funds_value()) };
        assert(f.has_value());
        return *f;
    }
};

struct TokenCreation : public View<8 + 8 + 5 + 2 + SIGLEN> {
private:
    uint16_t fee_raw() const
    {
        return readuint16(pos + 25);
    }

public:
    using View::View;
    // AccountId fromAccountId; 8 at 0
    // PinNonce pinNonce; 8 at 8
    // TokenName tokenName; 5 at 16
    // CompactUInt compactFee; 2 at 21
    // RecoverableSignature signature; 65 at 23
    // size: 88

    static_assert(size() == 8 + 8 + 5 + 2 + 65);
    AccountId origin_account_id() const
    {
        return AccountId(readuint64(pos));
    }
    PinNonce pin_nonce() const
    {
        Reader r({ pos + 8, pos + 16 });
        return PinNonce(r);
    }

    TokenName token_name() const
    {
        return TokenName { View<5>(pos + 20) };
    }
    CompactUInt compact_fee_throw() const
    {
        return CompactUInt::from_value_throw(fee_raw());
    }
    auto signature() const { return View<65>(pos + 27); }
};

struct Transfer : public View<34 + SIGLEN> {
private:
    uint16_t fee_raw() const
    {
        return readuint16(pos + 16);
    }

public:
    AccountId origin_account_id() const
    {
        return AccountId(readuint64(pos));
    }
    PinNonce pin_nonce() const
    {
        Reader r({ pos + 8, pos + 16 });
        return PinNonce(r);
    }
    PinHeight pin_height(PinFloor pinFloor) const
    {
        return pin_nonce().pin_height_from_floored(pinFloor);
    }
    CompactUInt compact_fee_throw() const
    {
        return CompactUInt::from_value_throw(fee_raw());
    }

    CompactUInt compact_fee_assert() const
    {
        return CompactUInt::from_value_assert(fee_raw());
    }
    [[nodiscard]] Funds_uint64 amount_throw() const
    {
        return Funds_uint64::from_value_throw(readuint64(pos + 26));
    }

    Funds_uint64 fee_throw() const
    {
        return compact_fee_throw().uncompact();
    }
    AccountId toAccountId() const
    {
        return AccountId(readuint64(pos + 18));
    }
    auto signature() const { return View<65>(pos + 34); }
    TransactionId txid(PinHeight pinHeight) const
    {
        PinNonce pn = pin_nonce();
        return { origin_account_id(), pinHeight, pn.id };
    }

    using View::View;
};
struct TokenTransfer : public Transfer {
    TokenId id;
    TokenTransfer(Transfer t, TokenId id)
        : Transfer(std::move(t))
        , id(id) { };
};

struct WartTransfer : public TokenTransfer {

    WartTransfer(const uint8_t* p)
        : WartTransfer(Transfer { p })
    {
    }
    WartTransfer(const Transfer& v)
        : TokenTransfer(v, TokenId(0))
    {
    }
    Wart amount_throw() const
    {
        return Wart::from_funds_throw(Transfer::amount_throw());
    }
};

struct Order : public View<30 + SIGLEN> {
private:
    TokenId tokenId;
    uint16_t fee_raw() const
    {
        return readuint16(pos + 16);
    }

public:
    Order(const uint8_t* pos, TokenId tokenId)
        : View(pos)
        , tokenId(tokenId) { };
    auto token_id() const { return tokenId; }
    AccountId origin_account_id() const
    {
        return AccountId(readuint64(pos));
    }
    PinNonce pin_nonce() const
    {
        Reader r({ pos + 8, pos + 16 });
        return PinNonce(r);
    }
    PinHeight pin_height(PinFloor pinFloor) const
    {
        return pin_nonce().pin_height_from_floored(pinFloor);
    }
    CompactUInt compact_fee_throw() const
    {
        return CompactUInt::from_value_throw(fee_raw());
    }

    CompactUInt compact_fee_assert() const
    {
        return CompactUInt::from_value_assert(fee_raw());
    }

    Funds_uint64 fee_throw() const
    {
        return compact_fee_throw().uncompact();
    }

    std::pair<bool, Funds_uint64> buy_amount_throw() const
    {
        auto v { readuint64(pos + 18) };
        bool buy { (v >> 63) != 0 };
        auto f { Funds_uint64::from_value_throw(v & 0x7FFFFFFFFFFFFFFFull) };
        return { buy, f };
    }
    Price_uint64 limit() const
    {
        return Price_uint64::from_uint32_throw(readuint32(pos + 26));
    }
    auto signature() const { return View<65>(pos + 30); }
    TransactionId txid(PinHeight pinHeight) const
    {
        PinNonce pn = pin_nonce();
        return { origin_account_id(), pinHeight, pn.id };
    }

    using View::View;
};

struct Cancelation : public View<24 + SIGLEN> {
private:
    TokenId _tokenId;
    uint16_t fee_raw() const
    {
        return readuint16(pos + 16);
    }

public:
    auto token_id() { return _tokenId; }
    Cancelation(const uint8_t* pos, TokenId tokenId)
        : View(pos)
        , _tokenId(tokenId)
    {
    }
    AccountId origin_account_id() const
    {
        return AccountId(readuint64(pos));
    }
    PinNonce pin_nonce() const
    {
        Reader r({ pos + 8, pos + 16 });
        return PinNonce(r);
    }
    PinHeight pin_height(PinFloor pinFloor) const
    {
        return pin_nonce().pin_height_from_floored(pinFloor);
    }
    PinNonce block_pin_nonce() const
    {
        Reader r({ pos + 18, pos + 24 });
        return PinNonce(r);
    }
    CompactUInt compact_fee_throw() const
    {
        return CompactUInt::from_value_throw(fee_raw());
    }

    CompactUInt compact_fee_assert() const
    {
        return CompactUInt::from_value_assert(fee_raw());
    }

    Funds_uint64 fee_throw() const
    {
        return compact_fee_throw().uncompact();
    }

    auto signature() const { return View<65>(pos + 24); }
    TransactionId txid(PinHeight pinHeight) const
    {
        PinNonce pn = pin_nonce();
        return { origin_account_id(), pinHeight, pn.id };
    }
    TransactionId block_txid(PinHeight pinHeight) const
    {
        PinNonce pn = pin_nonce();
        return { origin_account_id(), pinHeight, pn.id };
    }

    using View::View;
};

struct LiquidityAdd : public View<34 + SIGLEN> {
private:
    TokenId tokenId;
    uint16_t fee_raw() const
    {
        return readuint16(pos + 16);
    }

public:
    LiquidityAdd(const uint8_t* pos, TokenId tokenId)
        : View(pos)
        , tokenId(tokenId) { };
    AccountId origin_account_id() const
    {
        return AccountId(readuint64(pos));
    }
    PinNonce pin_nonce() const
    {
        Reader r({ pos + 8, pos + 16 });
        return PinNonce(r);
    }
    PinHeight pinHeight(PinFloor pinFloor) const
    {
        return pin_nonce().pin_height_from_floored(pinFloor);
    }
    CompactUInt compact_fee_throw() const
    {
        return CompactUInt::from_value_throw(fee_raw());
    }

    CompactUInt compact_fee_assert() const
    {
        return CompactUInt::from_value_assert(fee_raw());
    }

    Funds_uint64 fee_throw() const
    {
        return compact_fee_throw().uncompact();
    }
    Funds_uint64 amountQuoteWART() const
    {
        return Funds_uint64::from_value_throw(readuint64(pos + 18));
    }
    Funds_uint64 amountBase() const
    {
        return Funds_uint64::from_value_throw(readuint64(pos + 26));
    }
    auto signature() const { return View<65>(pos + 34); }
    TransactionId txid(PinHeight pinHeight) const
    {
        PinNonce pn = pin_nonce();
        return { origin_account_id(), pinHeight, pn.id };
    }

    using View::View;
};

struct LiquidityRemove : public View< 26 + SIGLEN> {
private:
    TokenId tokenId;
    uint16_t fee_raw() const
    {
        return readuint16(pos + 16);
    }

public:
    LiquidityRemove(const uint8_t* pos, TokenId tokenId)
        : View(pos)
        , tokenId(tokenId) { };
    AccountId origin_account_id() const
    {
        return AccountId(readuint64(pos));
    }
    PinNonce pin_nonce() const
    {
        Reader r({ pos + 8, pos + 16 });
        return PinNonce(r);
    }
    PinHeight pinHeight(PinFloor pinFloor) const
    {
        return pin_nonce().pin_height_from_floored(pinFloor);
    }
    CompactUInt compact_fee_throw() const
    {
        return CompactUInt::from_value_throw(fee_raw());
    }

    CompactUInt compact_fee_assert() const
    {
        return CompactUInt::from_value_assert(fee_raw());
    }

    Funds_uint64 fee_throw() const
    {
        return compact_fee_throw().uncompact();
    }
    Funds_uint64 amountPooltoken() const
    {
        return Funds_uint64::from_value_throw(readuint64(pos + 18));
    }
    auto signature() const { return View<65>(pos + 26); }
    TransactionId txid(PinHeight pinHeight) const
    {
        PinNonce pn = pin_nonce();
        return { origin_account_id(), pinHeight, pn.id };
    }

    using View::View;
};

// struct Transfer {
//     // byte layout
//     //  0: fromId
//     //  8: pinNonce
//     // 16: compactFee
//     // 18: toId
//     // 26: amout
//     // 34: signature
//     // 99: end
//     AccountId fromId;
//     PinNonce pinNonce;
//     CompactUInt compactFee;
//     AccountId toId;
//     Funds_uint64 amount;
//     RecoverableSignature signature;
//     [[nodiscard]] PinHeight pin_height(PinFloor pinFloor) const
//     {
//         return pinNonce.pin_height(pinFloor);
//     }
//     [[nodiscard]] Funds_uint64 fee() const
//     {
//         return compactFee.uncompact();
//     }
//     [[nodiscard]] TransactionId txid(PinHeight pinHeight) const
//     {
//         return { fromId,
//             pinHeight, pinNonce.id };
//     }
//     Transfer(WartTransferView v)
//         : fromId(v.fromAccountId())
//         , pinNonce(v.pin_nonce())
//         , compactFee(v.compact_fee_throw())
//         , toId(v.toAccountId())
//         , amount(v.amount_throw())
//         , signature(v.signature())
//     {
//     }
// };


}
}
}

// constexpr static size_t CancelationSize { 16 + SIGLEN }; // TODO
// constexpr static size_t TokenCreationSize { 8 + 8 + 5 + 2 + SIGLEN };
