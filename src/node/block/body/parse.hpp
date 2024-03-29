#pragma once

#include "block/body/transaction_id.hpp"
#include "block/body/view.hpp"
#include "crypto/crypto.hpp"

struct TransferView : public View<BodyView::TransferSize> {
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
    CompactUInt compact_fee() const
    {
        return readuint16(pos + 16);
    }
    Funds fee() const
    {
        return compact_fee().uncompact();
    }
    AccountId toAccountId() const
    {
        return AccountId(readuint64(pos + 18));
    }
    Funds amount() const
    {
        return Funds { readuint64(pos + 26) };
    }
    auto signature() const { return View<65>(pos + 34); }
    static_assert(65 == BodyView::SIGLEN);
    TransactionId txid(PinHeight pinHeight) const
    {
        PinNonce pn = pin_nonce();
        return { fromAccountId(), pinHeight, pn.id };
    }

    TransferView(const uint8_t* pos)
        : View(pos) {}
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
        , compactFee(v.compact_fee())
        , toId(v.toAccountId())
        , amount(v.amount())
        , signature(v.signature()) {}
};

struct RewardView : public View<BodyView::RewardSize> {
    RewardView(const uint8_t* pos, uint16_t i)
        : View(pos)
        , offset(i)
    {
    }
    AccountId account_id() const
    {
        return AccountId(readuint64(pos));
    }
    Funds amount() const
    {
        return Funds(readuint64(pos + 8));
    }
    const uint16_t offset; // index in block
};

inline TransferView BodyView::get_transfer(size_t i) const
{
    assert(i < nTransfers);
    return data() + offsetTransfers + i * TransferView::size();
}

inline RewardView BodyView::reward() const
{
    return {data() + offsetReward , 0};
}

inline Funds BodyView::fee_sum() const
{
    Funds sum{0};
    for (auto t : transfers()) 
        sum += t.amount();
    return sum;
}
inline AddressView BodyView::get_address(size_t i) const
{
    static_assert(AddressView::size() == BodyView::AddressSize);
    assert(i < nAddresses);
    return AddressView(data() + offsetAddresses + i * AddressView::size());
}
inline AddressView BodyView::Addresses::Iterator::operator*() const {
    return bv.get_address(i);
}
inline TransferView BodyView::Transfers::Iterator::operator*() const {
    return bv.get_transfer(i);
}
