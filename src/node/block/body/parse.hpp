#pragma once

#include "block/body/transaction_id.hpp"
#include "block/body/view.hpp"
#include "crypto/crypto.hpp"
#include "defi/token/token.hpp"
#include "general/reader.hpp"

struct TokenCreationView : public View<BodyView::TokenCreationSize> {
private:
    uint16_t fee_raw() const
    {
        return readuint16(pos + 25);
    }

public:
    using View<BodyView::TokenCreationSize>::View;
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
    CompactUInt compact_fee_trow() const
    {
        return CompactUInt::from_value_throw(fee_raw());
    }
    auto signature() const { return View<65>(pos + 27); }
};
struct TransferView : public View<BodyView::TransferSize> {
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
    PinHeight pinHeight(PinFloor pinFloor) const
    {
        return pin_nonce().pin_height(pinFloor);
    }
    CompactUInt compact_fee_trow() const
    {
        return CompactUInt::from_value_throw(fee_raw());
    }

    CompactUInt compact_fee_assert() const
    {
        return CompactUInt::from_value_assert(fee_raw());
    }

    Funds fee_throw() const
    {
        return compact_fee_trow().uncompact();
    }
    AccountId toAccountId() const
    {
        return AccountId(readuint64(pos + 18));
    }
    Funds amount_throw() const
    {
        return Funds::from_value_throw(readuint64(pos + 26));
    }
    auto signature() const { return View<65>(pos + 34); }
    static_assert(65 == BodyView::SIGLEN);
    TransactionId txid(PinHeight pinHeight) const
    {
        PinNonce pn = pin_nonce();
        return { fromAccountId(), pinHeight, pn.id };
    }

    TransferView(const uint8_t* pos)
        : View(pos)
    {
    }
    const uint8_t* data() const { return pos; }
};

struct Transfer {
    // byte layout
    //  0: fromId
    //  8: pinNonce
    // 16: compactFee
    // 18: toId
    // 26: amout
    // 34: signature
    // 99: end
    AccountId fromId;
    PinNonce pinNonce;
    CompactUInt compactFee;
    AccountId toId;
    Funds amount;
    RecoverableSignature signature;
    [[nodiscard]] PinHeight pin_height(PinFloor pinFloor) const
    {
        return pinNonce.pin_height(pinFloor);
    }
    [[nodiscard]] Funds fee() const
    {
        return compactFee.uncompact();
    }
    [[nodiscard]] TransactionId txid(PinHeight pinHeight) const
    {
        return { fromId,
            pinHeight, pinNonce.id };
    }
    Transfer(TransferView v)
        : fromId(v.fromAccountId())
        , pinNonce(v.pin_nonce())
        , compactFee(v.compact_fee_trow())
        , toId(v.toAccountId())
        , amount(v.amount_throw())
        , signature(v.signature())
    {
    }
};

struct RewardView : public View<BodyView::RewardSize> {
private:
    auto funds_value() const
    {
        return readuint64(pos + 8);
    }

public:
    RewardView(const uint8_t* pos, uint16_t i)
        : View(pos)
        , offset(i)
    {
    }
    AccountId account_id() const
    {
        return AccountId(readuint64(pos));
    }
    Funds amount_throw() const
    {
        return Funds::from_value_throw(funds_value());
    }
    Funds amount_assert() const
    {
        auto f { Funds::from_value(funds_value()) };
        assert(f.has_value());
        return *f;
    }
    const uint16_t offset; // index in block
};

inline TransferView BodyView::get_transfer(size_t i) const
{
    assert(i < nTransfers);
    return data() + offsetTransfers + i * TransferView::size();
}

inline TokenCreationView BodyView ::get_new_token(size_t i) const
{
    assert(i < nNewTokens);
    return TokenCreationView { data() + offsetNewTokens + i * TokenCreationView::size() };
}

inline RewardView BodyView::reward() const
{
    return { data() + offsetReward, 0 };
}

inline Funds BodyView::fee_sum_assert() const
{
    Funds sum { Funds::zero() };
    for (auto t : transfers())
        sum.add_assert(t.compact_fee_assert().uncompact());
    return sum;
}
inline AddressView BodyView::get_address(size_t i) const
{
    static_assert(AddressView::size() == BodyView::AddressSize);
    assert(i < nAddresses);
    return AddressView(data() + offsetAddresses + i * AddressView::size());
}
inline AddressView BodyView::Addresses::Iterator::operator*() const
{
    return bv.get_address(i);
}
inline TransferView BodyView::Transfers::Iterator::operator*() const
{
    return bv.get_transfer(i);
}
inline TokenCreationView BodyView::NewTokens::Iterator::operator*() const
{
    return bv.get_new_token(i);
}
