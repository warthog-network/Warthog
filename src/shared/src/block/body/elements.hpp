#pragma once
#include "block/body/transaction_id.hpp"
#include "elements_fwd.hpp"
#include "general/base_elements.hpp"
#include "general/serializer_fwd.hxx"
#include "general/structured_reader_fwd.hpp"
namespace block {
namespace body {

template <typename... Ts>
struct Combined : public Ts... {
    Combined(StructuredReader& r)
        : Ts(r)...
    {
    }
    Combined(Ts... ts)
        : Ts(std::move(ts))...
    {
    }
    void serialize(Serializer auto&& s) const
    {
        (s << ... << static_cast<const Ts*>(this)->get());
    }
    void write(Writer& w) const
    {
        serialize(w);
    }
    static constexpr size_t byte_size()
    {
        return (Ts::byte_size() + ...);
    }
    void append_merkle_leaves(std::vector<Hash>& out) const
    {
        out.push_back(hashSHA256(*this));
    }
};
template <typename... Ts>
struct SignedCombined : public Combined<OriginAccIdEl, PinNonceEl, CompactFeeEl, Ts..., SignatureEl> {
    using Combined<OriginAccIdEl, PinNonceEl, CompactFeeEl, Ts..., SignatureEl>::Combined;
    [[nodiscard]] TransactionId txid(PinHeight pinHeight) const
    {
        PinNonce pn = this->pin_nonce();
        return { this->origin_account_id(), pinHeight, pn.id };
    }
    [[nodiscard]] TransactionId txid_from_floored(PinFloor pinFloor) const
    {
        PinNonce pn = this->pin_nonce();
        auto pinHeight { pn.pin_height_from_floored(pinFloor) };
        return { this->origin_account_id(), pinHeight, pn.id };
    }
    void append_txids(std::vector<TransactionId>& txids, PinFloor pf, PinHeight minPinheight) const
    {
        TransactionId txid { txid_from_floored(pf) };
        if (txid.pinHeight < minPinheight)
            return;
        txids.push_back(txid);
    }
};

struct CancelationBase : public SignedCombined<CancelHeightEl, CancelNonceEl> {
    using SignedCombined<CancelHeightEl, CancelNonceEl>::SignedCombined;
    TransactionId canceled_txid() const
    {
        return { origin_account_id(), cancel_height(), cancel_nonceid() };
    }
};

}
}
