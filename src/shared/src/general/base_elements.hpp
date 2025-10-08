#pragma once
#include "base_elements_fwd.hpp"
#include "block/body/account_id.hpp"
#include "block/body/nonce.hpp"
#include "block/body/transaction_id.hpp"
#include "block/chain/height.hpp"
#include "block/chain/history/index.hpp"
#include "crypto/crypto.hpp"
#include "defi/token/token.hpp"
#include "defi/uint64/price.hpp"
#include "general/compact_uint.hpp"
#include "general/funds.hpp"

namespace enumerated {
template <size_t i, typename Element>
struct Annotated : public Element {
    using Element::Element;
    Annotated(Element e)
        : Element(std::move(e))
    {
    }
    template <size_t j>
    requires(j == i)
    auto& get_at() const
    {
        return this->get();
    }
};

template <size_t i, typename... Elements>
struct CombineElementsEnumerated;

template <size_t i, typename Element, typename... Elements>
struct CombineElementsEnumerated<i, Element, Elements...> : public Element, public CombineElementsEnumerated<i + 1, Elements...> {
    using parent_t = CombineElementsEnumerated<i + 1, Elements...>;
    template <size_t j>
    requires(j == i)
    auto& get_at() const
    {
        return static_cast<const Element*>(this)->get();
    }
    template <size_t j>
    auto& get_at() const
    {
        return static_cast<const parent_t*>(this)->template get_at<j>();
    }
    CombineElementsEnumerated(Reader& r)
        : Element(r)
        , parent_t(r)
    {
    }
    CombineElementsEnumerated(Element::data_t t, Elements::data_t... ts)
        : Element(std::move(t))
        , parent_t(std::move(ts)...)
    {
    }
    [[nodiscard]] size_t byte_size() const { return (Elements::byte_size() + ...); }
    template <typename E>
    auto& get() const
    {
        return static_cast<const E*>(this)->get();
    }
    void serialize(Serializer auto&& s) const
    {
        ((s << static_cast<const Element*>(this)->get())
            << ... << static_cast<const Elements*>(this)->get());
    }
};

template <size_t i>
struct CombineElementsEnumerated<i> {
    CombineElementsEnumerated(Reader&) { }
    CombineElementsEnumerated() { }
};

// template <typename... Elements>
// struct Enumerated {
//     template <typename T>
//     struct CombineElementsEnumerated;
//
//     template <size_t... Is>
//     struct CombineElementsEnumerated<std::index_sequence<Is...>> : public Annotated<Is, Elements>... {
//         using combined_t = CombineElementsEnumerated;
//         CombineElementsEnumerated(Reader& r)
//             : Annotated<Is, Elements>(r)...
//         {
//         }
//         CombineElementsEnumerated(Elements::data_t... ts)
//             : Annotated<Is, Elements>(std::move(ts))...
//         {
//         }
//         [[nodiscard]] size_t byte_size() const { return (Elements::byte_size() + ...); }
//         template <typename Element>
//         auto& get() const
//         {
//             return static_cast<const Element*>(this)->get();
//         }
//         void serialize(Serializer auto&& s) const
//         {
//             (s << ... << static_cast<const Elements*>(this)->get());
//         }
//     };
// };

}

template <typename... Elements>
struct CombineElements : public enumerated::CombineElementsEnumerated<0, Elements...> {
    using enumerated::CombineElementsEnumerated<0, Elements...>::CombineElementsEnumerated;
};

template <typename T>
concept HasStaticBytesize = requires {
    { T::byte_size() } -> std::same_as<size_t>;
};

template <typename T>
concept HasNonStaticBytesize = requires(T t) {
    { t.byte_size() } -> std::same_as<void>;
};

template <typename T>
struct ElementBase {
    [[nodiscard]] size_t byte_size() const { return data.byte_size(); }
    const T& get() const { return data; }

    ElementBase(Reader& r)
        : data(r)
    {
    }
    ElementBase(T t)
        : data(std::move(t))
    {
    }
    using base_t = ElementBase;
    using data_t = T;

protected:
    T data;
};

template <HasStaticBytesize T>
struct ElementBase<T> {
    static constexpr size_t byte_size() { return T::byte_size(); }
    const T& get() const { return data; }

    ElementBase(Reader& r)
        : data(r)
    {
    }
    ElementBase(T t)
        : data(std::move(t))
    {
    }
    using data_t = T;

protected:
    T data;
};

struct AccountIdEl : public ElementBase<AccountId> {
    using ElementBase::ElementBase;
    [[nodiscard]] const AccountId& account_id() const { return data; }
};
struct ToAccIdEl : public ElementBase<AccountId> {
    using ElementBase::ElementBase;
    [[nodiscard]] const AccountId& to_id() const { return data; }
};
struct OriginAccIdEl : public ElementBase<AccountId> {
    using ElementBase::ElementBase;
    [[nodiscard]] const AccountId& origin_account_id() const { return data; }
};

struct AssetSupplyEl : public ElementBase<FundsDecimal> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& supply() const { return data; }
};
struct ToAddrEl : public ElementBase<Address> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& to_addr() const { return data; }
};
struct CreatorAddrEl : public ElementBase<Address> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& creator_addr() const { return data; }
};

struct WartEl : public ElementBase<Wart> {
    using ElementBase::ElementBase;
    [[nodiscard]] const Wart& wart() const { return data; }
};

struct QuoteWartEl : public ElementBase<Wart> {
    using ElementBase::ElementBase;
    [[nodiscard]] const Wart& quote_wart() const { return data; }
};
struct FillEl : public ElementBase<Funds_uint64> {
    using ElementBase::ElementBase;
    [[nodiscard]] const Funds_uint64& amount() const { return data; }
};
struct AmountEl : public ElementBase<Funds_uint64> {
    using ElementBase::ElementBase;
    [[nodiscard]] const Funds_uint64& amount() const { return data; }
};
struct BaseAmountEl : public ElementBase<Funds_uint64> {
    using ElementBase::ElementBase;
    [[nodiscard]] const Funds_uint64& base_amount() const { return data; }
};
struct CancelNonceEl : public ElementBase<NonceId> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& cancel_nonceid() const { return data; }
};
struct CancelHeightEl : public ElementBase<PinHeight> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& cancel_height() const { return data; }
};

struct CancelTxidEl : public ElementBase<TransactionId> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& cancel_txid() const { return data; }
};
struct OrderIdEl : public ElementBase<HistoryId> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& order_id() const { return data; }
};
struct ReferredHistoryIdEl : public ElementBase<HistoryId> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& referred_history_id() const { return data; }
};
struct PinNonceEl : public ElementBase<PinNonce> {
    using ElementBase::ElementBase;
    [[nodiscard]] const PinNonce& pin_nonce() const { return data; }
};
struct CompactFeeEl : public ElementBase<CompactUInt> {
    using ElementBase::ElementBase;
    [[nodiscard]] const CompactUInt& compact_fee() const { return data; }
    [[nodiscard]] Wart fee() const { return data.uncompact(); }
};
struct AssetPrecisionEl : public ElementBase<AssetPrecision> {
    using ElementBase::ElementBase;
    [[nodiscard]] const AssetPrecision& token_precision() const { return data; }
};
struct AssetNameEl : public ElementBase<AssetName> {
    using ElementBase::ElementBase;
    [[nodiscard]] const AssetName& asset_name() const { return data; }
};
struct AssetHashEl : public ElementBase<AssetHash> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& asset_hash() const { return data; }
};
struct BoolElBase {
    BoolElBase(uint8_t v)
        : b(v != 0)
    {
        if (v > 1)
            throw Error(EINVBUY);
    }
    using data_t = bool;
    static constexpr size_t byte_size() { return 1; }
    const bool& get() const { return b; }

private:
    bool b;
};

struct LiquidityFlagEl : public BoolElBase { // specifies whether a token transfer is the asset itself or the corresponding asset's pool token (there are only pools with WART quote).
    using BoolElBase::BoolElBase;
    bool is_liquidity() const
    {
        return get();
    }
};

struct NonWartTokenIdEl : public ElementBase<NonWartTokenId> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& token_id() const { return data; }
};
struct AssetIdEl : public ElementBase<AssetId> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& asset_id() const { return data; }
};
struct SignatureEl : public ElementBase<RecoverableSignature> {
    using ElementBase::ElementBase;
    [[nodiscard]] const RecoverableSignature& signature() const { return data; }
};
struct LimitPriceEl : public ElementBase<Price_uint64> {
    using ElementBase::ElementBase;
    [[nodiscard]] const Price_uint64& limit() const { return data; }
};
struct BuyEl : BoolElBase {
    using BoolElBase::BoolElBase;
    bool buy() const { return get(); }
};

struct PinHeightEl : public ElementBase<PinHeight> {
    using ElementBase::ElementBase;
    [[nodiscard]] const PinHeight& pin_height() const { return data; }
};

struct TransactionIdEl : public ElementBase<TransactionId> {
    using ElementBase::ElementBase;
    [[nodiscard]] const TransactionId& txid() const { return data; }
    [[nodiscard]] AccountId from_id() const { return txid().accountId; }
    [[nodiscard]] PinHeight pin_height() const { return txid().pinHeight; }
    [[nodiscard]] NonceId nonce_id() const { return txid().nonceId; }
};

struct NonceIdEl : public ElementBase<NonceId> {
    using ElementBase::ElementBase;
    [[nodiscard]] const NonceId& nonce_id() const { return data; }
};

struct NonceReservedEl : public ElementBase<NonceReserved> {
    using ElementBase::ElementBase;
    [[nodiscard]] const NonceReserved& nonce_reserved() const { return data; }
};

struct BaseEl : public ElementBase<Funds_uint64> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& base() const { return data; }
};

struct QuoteEl : public ElementBase<Wart> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& quote() const { return data; }
};
struct SharesEl : public ElementBase<Funds_uint64> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& shares() const { return data; }
};
