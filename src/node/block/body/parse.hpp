#pragma once

#include "block/body/transaction_id.hpp"
#include "block/body/view.hpp"
#include "crypto/crypto.hpp"
#include "defi/token/token.hpp"
#include "defi/uint64/price.hpp"
#include "general/reader.hpp"

struct TokenCreationView : public View<BodyStructure::TokenCreationSize> {
private:
    uint16_t fee_raw() const
    {
        return readuint16(pos + 25);
    }

public:
    using View<BodyStructure::TokenCreationSize>::View;
    // AccountId fromAccountId; 8 at 0
    // PinNonce pinNonce; 8 at 8
    // TokenName tokenName; 5 at 16
    // CompactUInt compactFee; 2 at 21
    // RecoverableSignature signature; 65 at 23
    // size: 88

    static_assert(size() == 8 + 8 + 5 + 2 + 65);
    AccountId fromAccountId() const
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
struct TransferView : public View<BodyStructure::TransferSize> {
private:
    uint16_t fee_raw() const
    {
        return readuint16(pos + 16);
    }

public:
    AccountId fromAccountId() const
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
    static_assert(65 == BodyStructure::SIGLEN);
    TransactionId txid(PinHeight pinHeight) const
    {
        PinNonce pn = pin_nonce();
        return { fromAccountId(), pinHeight, pn.id };
    }

    using View::View;
};

struct TokenTransferView : public TransferView {
    TokenTransferView(TransferView v, TokenId id)
        : TransferView(std::move(v))
        , _tokenId(id)
    {
    }
    auto token_id() { return _tokenId; }

private:
    TokenId _tokenId;
};

struct WartTransferView : public TokenTransferView {
    WartTransferView(TransferView v)
        : TokenTransferView(std::move(v), TokenId(0))
    {
    }
    Wart amount_throw() const
    {
        return Wart::from_funds_throw(TransferView::amount_throw());
    }
};

struct OrderView : public View<BodyStructure::OrderSize> {
private:
    TokenId tokenId;
    uint16_t fee_raw() const
    {
        return readuint16(pos + 16);
    }

public:
    OrderView(const uint8_t* pos, TokenId tokenId)
        : View(pos)
        , tokenId(tokenId) { };
    auto token_id() const { return tokenId; }
    AccountId account_id() const
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
    CompactUInt compact_fee_trow() const
    {
        return CompactUInt::from_value_throw(fee_raw());
    }

    CompactUInt compact_fee_assert() const
    {
        return CompactUInt::from_value_assert(fee_raw());
    }

    Funds_uint64 fee_throw() const
    {
        return compact_fee_trow().uncompact();
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
    static_assert(65 == BodyStructure::SIGLEN);
    TransactionId txid(PinHeight pinHeight) const
    {
        PinNonce pn = pin_nonce();
        return { account_id(), pinHeight, pn.id };
    }

    using View::View;
};

struct CancelationView : public View<BodyStructure::CancelSize> {
private:
    TokenId _tokenId;
    uint16_t fee_raw() const
    {
        return readuint16(pos + 16);
    }

public:
    auto token_id() { return _tokenId; }
    CancelationView(const uint8_t* pos, TokenId tokenId)
        : View(pos)
        , _tokenId(tokenId)
    {
    }
    AccountId account_id() const
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
    CompactUInt compact_fee_trow() const
    {
        return CompactUInt::from_value_throw(fee_raw());
    }

    CompactUInt compact_fee_assert() const
    {
        return CompactUInt::from_value_assert(fee_raw());
    }

    Funds_uint64 fee_throw() const
    {
        return compact_fee_trow().uncompact();
    }

    auto signature() const { return View<65>(pos + 24); }
    TransactionId txid(PinHeight pinHeight) const
    {
        PinNonce pn = pin_nonce();
        return { account_id(), pinHeight, pn.id };
    }
    TransactionId block_txid(PinHeight pinHeight) const
    {
        PinNonce pn = pin_nonce();
        return { account_id(), pinHeight, pn.id };
    }

    using View::View;
};

struct LiquidityAddView : public View<BodyStructure::LiquidityAddSize> {
private:
    TokenId tokenId;
    uint16_t fee_raw() const
    {
        return readuint16(pos + 16);
    }

public:
    LiquidityAddView(const uint8_t* pos, TokenId tokenId)
        : View(pos)
        , tokenId(tokenId) { };
    AccountId account_id() const
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
    CompactUInt compact_fee_trow() const
    {
        return CompactUInt::from_value_throw(fee_raw());
    }

    CompactUInt compact_fee_assert() const
    {
        return CompactUInt::from_value_assert(fee_raw());
    }

    Funds_uint64 fee_throw() const
    {
        return compact_fee_trow().uncompact();
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
    static_assert(65 == BodyStructure::SIGLEN);
    TransactionId txid(PinHeight pinHeight) const
    {
        PinNonce pn = pin_nonce();
        return { account_id(), pinHeight, pn.id };
    }

    using View::View;
};

struct LiquidityRemoveView : public View<BodyStructure::LiquidityRemoveSize> {
private:
    TokenId tokenId;
    uint16_t fee_raw() const
    {
        return readuint16(pos + 16);
    }

public:
    LiquidityRemoveView(const uint8_t* pos, TokenId tokenId)
        : View(pos)
        , tokenId(tokenId) { };
    AccountId account_id() const
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
    CompactUInt compact_fee_trow() const
    {
        return CompactUInt::from_value_throw(fee_raw());
    }

    CompactUInt compact_fee_assert() const
    {
        return CompactUInt::from_value_assert(fee_raw());
    }

    Funds_uint64 fee_throw() const
    {
        return compact_fee_trow().uncompact();
    }
    Funds_uint64 amountPooltoken() const
    {
        return Funds_uint64::from_value_throw(readuint64(pos + 18));
    }
    auto signature() const { return View<65>(pos + 26); }
    static_assert(65 == BodyStructure::SIGLEN);
    TransactionId txid(PinHeight pinHeight) const
    {
        PinNonce pn = pin_nonce();
        return { account_id(), pinHeight, pn.id };
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

struct RewardView : public View<BodyStructure::RewardSize> {
private:
    auto funds_value() const
    {
        return readuint64(pos + 8);
    }

public:
    RewardView(const uint8_t* pos)
        : View(pos)
    {
    }
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

inline WartTransferView BodyView::get_wart_transfer(size_t i) const
{
    assert(i < bodyStructure.nTransfers);
    return WartTransferView { data() + bodyStructure.offsetTransfers + i * WartTransferView::size() };
}

inline TokenCreationView BodyView ::get_new_token(size_t i) const
{
    assert(i < bodyStructure.nNewTokens);
    return TokenCreationView { data() + bodyStructure.offsetNewTokens + i * TokenCreationView::size() };
}

inline RewardView BodyView::reward() const
{
    return { data() + bodyStructure.offsetReward };
}

inline Funds_uint64 BodyView::fee_sum_assert() const
{
    Funds_uint64 sum { Funds_uint64::zero() };
    for (auto t : wart_transfers())
        sum.add_assert(t.compact_fee_assert().uncompact());
    return sum;
}
inline AddressView BodyView::get_address(size_t i) const
{
    static_assert(AddressView::size() == BodyStructure::AddressSize);
    assert(i < bodyStructure.nAddresses);
    return AddressView(data() + bodyStructure.offsetAddresses + i * AddressView::size());
}
inline AddressView BodyView::Addresses::Iterator::operator*() const
{
    return bv.get_address(i);
}
inline WartTransferView BodyView::WartTransfers::Iterator::operator*() const
{
    return bv.get_wart_transfer(i);
}
inline TokenCreationView BodyView::NewTokens::Iterator::operator*() const
{
    return bv.get_new_token(i);
}

inline auto BodyView::foreach_token(auto lambda) const
{
    for (auto& ts : bodyStructure.tokensSections)
        lambda(BodyStructure::TokenSectionView { ts, data() });
}

inline auto BodyStructure::TokenSectionView::foreach_transfer(auto lambda) const
{
    for (size_t i = 0; i < ts.nTransfers; ++i)
        lambda(TokenTransferView { TransferView { dataBody + ts.transfersOffset + i * TransferView::size() }, ts.tokenId });
}

inline auto BodyStructure::TokenSectionView::foreach_order(auto lambda) const
{
    for (size_t i = 0; i < ts.nOrders; ++i)
        lambda(OrderView { dataBody + ts.ordersOffset + i * OrderView::size(), ts.tokenId });
}

inline auto BodyStructure::TokenSectionView::foreach_order_cancelation(auto lambda) const
{
    for (size_t i = 0; i < ts.nCancelation; ++i)
        lambda(CancelationView { dataBody + ts.cancelationOffset + i * CancelationView::size(), ts.tokenId });
}

inline auto BodyStructure::TokenSectionView::foreach_liquidity_add(auto lambda) const
{
    for (size_t i = 0; i < ts.nLiquidityAdd; ++i)
        lambda(LiquidityAddView { dataBody + ts.liquidityAddBegin + i * LiquidityAddView::size(), ts.tokenId });
}
inline auto BodyStructure::TokenSectionView::foreach_liquidity_remove(auto lambda) const
{
    for (size_t i = 0; i < ts.nLiquidityRemove; ++i)
        lambda(LiquidityRemoveView { dataBody + ts.liquidityRemoveOffset + i * LiquidityRemoveView::size(), ts.tokenId });
}
