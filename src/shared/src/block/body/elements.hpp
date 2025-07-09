#pragma once
#include "block/body/transaction_id.hpp"
#include "elements_fwd.hpp"
#include "general/base_elements.hpp"
#include "general/compact_uint.hpp"
#include "general/reader.hpp"
#include "general/serializer_fwd.hxx"
namespace block {
namespace body {
// constexpr static size_t SIGLEN { 65 };
// constexpr static size_t AddressSize { 20 };
// constexpr static size_t TransferSize { 34 + SIGLEN };
// constexpr static size_t OrderSize { 30 + SIGLEN };
// constexpr static size_t CancelSize { 24 + SIGLEN };
// constexpr static size_t LiquidityAddSize { 34 + SIGLEN };
// constexpr static size_t LiquidityRemoveSize { 26 + SIGLEN };
// constexpr static size_t CancelationSize { 16 + SIGLEN }; // TODO
// constexpr static size_t TokenCreationSize { 8 + 8 + 5 + 2 + SIGLEN };

template <typename... Ts>
struct Combined : public Ts... {
    Combined(Reader& r)
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
    void append_txids(std::vector<TransactionId>& txids, PinFloor pf) const
    {
        txids.push_back(txid_from_floored(pf));
    }
};

// struct Reward : public View<16> {
// private:
//     auto funds_value() const
//     {
//         return readuint64(pos + 8);
//     }
//
// public:
//     using View::View;
//
//     AccountId account_id() const
//     {
//         return AccountId(readuint64(pos));
//     }
//     Wart amount_throw() const
//     {
//         return Wart::from_value_throw(funds_value());
//     }
//     Funds_uint64 amount_assert() const
//     {
//         auto f { Funds_uint64::from_value(funds_value()) };
//         assert(f.has_value());
//         return *f;
//     }
// };

// struct TokenCreation : public View<8 + 8 + 2 + 8 + 1 + 5 + SIGLEN> {
// public:
//     using View::View;
//     // AccountId fromAccountId; 8 at 0
//     // PinNonce pinNonce; 8 at 8
//     // CompactUInt compactFee; 2 at 16
//     // Funds_uint64 totalSupply; 8 at 18
//     // TokenPrecision tokenPrecission; 1 at 26
//     // TokenName tokenName; 5 at 27
//     // RecoverableSignature signature; 65 at 32
//     // size: 97
//
//     AccountId origin_account_id() const
//     {
//         return AccountId(readuint64(pos));
//     }
//     PinNonce pin_nonce() const
//     {
//         Reader r({ pos + 8, pos + 16 });
//         return PinNonce(r);
//     }
//
// private:
//     uint16_t fee_raw() const
//     {
//         return readuint16(pos + 16);
//     }
//
// public:
//     Funds_uint64 total_supply() const
//     {
//         return Funds_uint64 { readuint64(pos + 18) };
//     }
//     TokenPrecision precision() const
//     {
//         return TokenPrecision::from_number_throw(*(pos + 26));
//     }
//     TokenName token_name() const
//     {
//         return TokenName { View<5>(pos + 27) };
//     }
//     CompactUInt compact_fee_throw() const
//     {
//         return CompactUInt::from_value_throw(fee_raw());
//     }
//     auto signature() const { return View<65>(pos + 32); }
// };

// template <typename... Ts>
// struct MessageView : public View<(Ts::byte_size() + ...)> {
//     using parent_t = View<(Ts::byte_size() + ...)>;
//     using tuple_t = std::tuple<Ts...>;
//     using parent_t::parent_t;
//     template <size_t i>
//     using elem_t = std::tuple_element_t<i, tuple_t>;
//     static constexpr size_t L = sizeof...(Ts);
//
//     template <size_t i>
//     requires(i < L)
//     static constexpr size_t offset()
//     {
//         if constexpr (i == 0) {
//             return 0;
//         } else {
//             return elem_t<i - 1>::byte_size() + offset<i - 1>();
//         }
//     }
//     template <size_t i>
//     requires(i < L)
//     auto get() const
//     {
//         constexpr size_t o { offset<i>() };
//         constexpr size_t N { parent_t::size() };
//         static_assert(o < N);
//         Reader r { std::span<const uint8_t> { parent_t::data() + o, N - o } };
//         return elem_t<i> { r };
//     }
// };
//
// struct Transfer : public MessageView<
//                       AccountId,
//                       PinNonce,
//                       CompactUInt,
//                       AccountId,
//                       Funds_uint64,
//                       View<65>> {
//
//     using MessageView::MessageView;
//     AccountId origin_account_id() const { return get<0>(); }
//     PinNonce pin_nonce() const { return get<1>(); }
//     CompactUInt fee_raw() const { return get<2>(); }
//     AccountId toAccountId() const { return get<3>(); }
//     Funds_uint64 amount_throw() const { return get<4>(); }
//     View<65> signature() const { return get<5>(); }
//     TransactionId txid(PinHeight pinHeight) const
//     {
//         PinNonce pn = pin_nonce();
//         return { origin_account_id(), pinHeight, pn.id };
//     }
// };

// struct Transfer : public View<34 + SIGLEN> {
// private:
//     uint16_t fee_raw() const
//     {
//         return readuint16(pos + 16);
//     }
//
// public:
//     AccountId origin_account_id() const
//     {
//         return AccountId(readuint64(pos));
//     }
//     PinNonce pin_nonce() const
//     {
//         Reader r({ pos + 8, pos + 16 });
//         return PinNonce(r);
//     }
//     PinHeight pin_height(PinFloor pinFloor) const
//     {
//         return pin_nonce().pin_height_from_floored(pinFloor);
//     }
//     CompactUInt compact_fee_throw() const
//     {
//         return CompactUInt::from_value_throw(fee_raw());
//     }
//
//     CompactUInt compact_fee_assert() const
//     {
//         return CompactUInt::from_value_assert(fee_raw());
//     }
//     [[nodiscard]] Funds_uint64 amount_throw() const
//     {
//         return Funds_uint64::from_value_throw(readuint64(pos + 26));
//     }
//
//     Funds_uint64 fee_throw() const
//     {
//         return compact_fee_throw().uncompact();
//     }
//     AccountId toAccountId() const
//     {
//         return AccountId(readuint64(pos + 18));
//     }
//     auto signature() const { return View<65>(pos + 34); }
//     TransactionId txid(PinHeight pinHeight) const
//     {
//         PinNonce pn = pin_nonce();
//         return { origin_account_id(), pinHeight, pn.id };
//     }
//
//     using View::View;
// };

// struct Order : public MessageView<
//                    AccountId,
//                    PinNonce,
//                    CompactUInt,
//                    IsUint64,
//                    Price_uint64,
//                    View<65>> {
//     using MessageView::MessageView;
//     AccountId origin_account_id() const { return get<0>(); }
//     PinNonce pin_nonce() const { return get<1>(); }
//     CompactUInt fee_raw() const { return get<2>(); }
//     std::pair<bool, Funds_uint64> buy_amount_throw() const
//     {
//         auto v { get<3>().value() };
//         bool buy { (v >> 63) != 0 };
//         auto f { Funds_uint64::from_value_throw(v & 0x7FFFFFFFFFFFFFFFull) };
//         return { buy, f };
//     }
//     Price_uint64 limit() const { return get<4>(); }
//     View<65> signature() const { return get<5>(); }
//     TransactionId txid(PinHeight pinHeight) const
//     {
//         PinNonce pn = pin_nonce();
//         return { origin_account_id(), pinHeight, pn.id };
//     }
// };

// struct Order : public View<30 + SIGLEN> {
// private:
//     uint16_t fee_raw() const
//     {
//         return readuint16(pos + 16);
//     }
//
// public:
//     Order(const uint8_t* pos)
//         : View(pos)
//     {
//     }
//     AccountId origin_account_id() const
//     {
//         return AccountId(readuint64(pos));
//     }
//     PinNonce pin_nonce() const
//     {
//         Reader r({ pos + 8, pos + 16 });
//         return PinNonce(r);
//     }
//     PinHeight pin_height(PinFloor pinFloor) const
//     {
//         return pin_nonce().pin_height_from_floored(pinFloor);
//     }
//     CompactUInt compact_fee_throw() const
//     {
//         return CompactUInt::from_value_throw(fee_raw());
//     }
//
//     CompactUInt compact_fee_assert() const
//     {
//         return CompactUInt::from_value_assert(fee_raw());
//     }
//
//     Funds_uint64 fee_throw() const
//     {
//         return compact_fee_throw().uncompact();
//     }
//
//     std::pair<bool, Funds_uint64> buy_amount_throw() const
//     {
//         auto v { readuint64(pos + 18) };
//         bool buy { (v >> 63) != 0 };
//         auto f { Funds_uint64::from_value_throw(v & 0x7FFFFFFFFFFFFFFFull) };
//         return { buy, f };
//     }
//     Price_uint64 limit() const
//     {
//         return Price_uint64::from_uint32_throw(readuint32(pos + 26));
//     }
//     auto signature() const { return View<65>(pos + 30); }
//     TransactionId txid(PinHeight pinHeight) const
//     {
//         PinNonce pn = pin_nonce();
//         return { origin_account_id(), pinHeight, pn.id };
//     }
//
//     using View::View;
// };

// struct Cancelation : public View<24 + SIGLEN> {
// private:
//     uint16_t fee_raw() const
//     {
//         return readuint16(pos + 16);
//     }
//
// public:
//     Cancelation(const uint8_t* pos)
//         : View(pos)
//     {
//     }
//     AccountId origin_account_id() const
//     {
//         return AccountId(readuint64(pos));
//     }
//     PinNonce pin_nonce() const
//     {
//         Reader r({ pos + 8, pos + 16 });
//         return PinNonce(r);
//     }
//     PinHeight pin_height(PinFloor pinFloor) const
//     {
//         return pin_nonce().pin_height_from_floored(pinFloor);
//     }
//     PinNonce block_pin_nonce() const
//     {
//         Reader r({ pos + 18, pos + 24 });
//         return PinNonce(r);
//     }
//     CompactUInt compact_fee_throw() const
//     {
//         return CompactUInt::from_value_throw(fee_raw());
//     }
//
//     CompactUInt compact_fee_assert() const
//     {
//         return CompactUInt::from_value_assert(fee_raw());
//     }
//
//     Funds_uint64 fee_throw() const
//     {
//         return compact_fee_throw().uncompact();
//     }
//
//     auto signature() const { return View<65>(pos + 24); }
//     TransactionId txid(PinHeight pinHeight) const
//     {
//         PinNonce pn = pin_nonce();
//         return { origin_account_id(), pinHeight, pn.id };
//     }
//     TransactionId block_txid(PinHeight pinHeight) const
//     {
//         PinNonce pn = pin_nonce();
//         return { origin_account_id(), pinHeight, pn.id };
//     }
//
//     using View::View;
// };

// struct LiquidityAdd : public View<34 + SIGLEN> {
// private:
//     uint16_t fee_raw() const
//     {
//         return readuint16(pos + 16);
//     }
//
// public:
//     LiquidityAdd(const uint8_t* pos)
//         : View(pos)
//     {
//     }
//     AccountId origin_account_id() const
//     {
//         return AccountId(readuint64(pos));
//     }
//     PinNonce pin_nonce() const
//     {
//         Reader r({ pos + 8, pos + 16 });
//         return PinNonce(r);
//     }
//     PinHeight pinHeight(PinFloor pinFloor) const
//     {
//         return pin_nonce().pin_height_from_floored(pinFloor);
//     }
//     CompactUInt compact_fee_throw() const
//     {
//         return CompactUInt::from_value_throw(fee_raw());
//     }
//
//     CompactUInt compact_fee_assert() const
//     {
//         return CompactUInt::from_value_assert(fee_raw());
//     }
//
//     Funds_uint64 fee_throw() const
//     {
//         return compact_fee_throw().uncompact();
//     }
//     Funds_uint64 amountQuoteWART() const
//     {
//         return Funds_uint64::from_value_throw(readuint64(pos + 18));
//     }
//     Funds_uint64 amountBase() const
//     {
//         return Funds_uint64::from_value_throw(readuint64(pos + 26));
//     }
//     auto signature() const { return View<65>(pos + 34); }
//     TransactionId txid(PinHeight pinHeight) const
//     {
//         PinNonce pn = pin_nonce();
//         return { origin_account_id(), pinHeight, pn.id };
//     }
//
//     using View::View;
// };

// struct LiquidityRemove : public View<26 + SIGLEN> {
// private:
//     uint16_t fee_raw() const
//     {
//         return readuint16(pos + 16);
//     }
//
// public:
//     LiquidityRemove(const uint8_t* pos)
//         : View(pos)
//     {
//     }
//     AccountId origin_account_id() const
//     {
//         return AccountId(readuint64(pos));
//     }
//     PinNonce pin_nonce() const
//     {
//         Reader r({ pos + 8, pos + 16 });
//         return PinNonce(r);
//     }
//     PinHeight pinHeight(PinFloor pinFloor) const
//     {
//         return pin_nonce().pin_height_from_floored(pinFloor);
//     }
//     CompactUInt compact_fee_throw() const
//     {
//         return CompactUInt::from_value_throw(fee_raw());
//     }
//
//     CompactUInt compact_fee_assert() const
//     {
//         return CompactUInt::from_value_assert(fee_raw());
//     }
//
//     Funds_uint64 fee_throw() const
//     {
//         return compact_fee_throw().uncompact();
//     }
//     Funds_uint64 amountPooltoken() const
//     {
//         return Funds_uint64::from_value_throw(readuint64(pos + 18));
//     }
//     auto signature() const { return View<65>(pos + 26); }
//     TransactionId txid(PinHeight pinHeight) const
//     {
//         PinNonce pn = pin_nonce();
//         return { origin_account_id(), pinHeight, pn.id };
//     }
//
//     using View::View;
// };

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

// constexpr static size_t CancelationSize { 16 + SIGLEN }; // TODO
// constexpr static size_t TokenCreationSize { 8 + 8 + 5 + 2 + SIGLEN };
