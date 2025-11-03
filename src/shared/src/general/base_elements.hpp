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
#include "general/static_string.hpp"
#include "general/structured_reader.hpp"

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
    [[nodiscard]] size_t byte_size() const { return Element::byte_size() + (Elements::byte_size() + ...); }
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

    ElementBase(T t)
        : data(std::move(t))
    {
    }
    using data_t = T;
    using base_t = ElementBase;

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
    using base_t = ElementBase;

protected:
    T data;
};

template <StaticString tag, typename T>
using ElementBaseWithAnnotation = Tag<tag, ElementBase<T>>;

struct CompactFeeEl : public ElementBaseWithAnnotation<"compactFee", CompactUInt> {
    using parent_t::parent_t;
    [[nodiscard]] const CompactUInt& compact_fee() const { return data; }
    [[nodiscard]] Wart fee() const { return data.uncompact(); }
};

#define ELEMENTMAP(NO_ANNOTATE, ANNOTATE)                            \
    NO_ANNOTATE(AccountIdEl, AccountId, account_id)                  \
    NO_ANNOTATE(AmountEl, Funds_uint64, amount)                      \
    NO_ANNOTATE(AssetHashEl, AssetHash, asset_hash)                  \
    NO_ANNOTATE(AssetIdEl, AssetId, asset_id)                        \
    NO_ANNOTATE(AssetNameEl, AssetName, asset_name)                  \
    NO_ANNOTATE(AssetPrecisionEl, AssetPrecision, asset_precision)   \
    NO_ANNOTATE(AssetSupplyEl, FundsDecimal, supply)                 \
    NO_ANNOTATE(BaseAmountEl, Funds_uint64, base_amount)             \
    NO_ANNOTATE(BaseEl, Funds_uint64, base)                          \
    NO_ANNOTATE(CancelHeightEl, PinHeight, cancel_height)            \
    NO_ANNOTATE(CancelNonceEl, NonceId, cancel_nonceid)              \
    NO_ANNOTATE(CancelTxidEl, TransactionId, cancel_txid)            \
    NO_ANNOTATE(CreatorAddrEl, Address, creator_addr)                \
    NO_ANNOTATE(FillEl, Funds_uint64, amount)                        \
    NO_ANNOTATE(LimitPriceEl, Price_uint64, limit)                   \
    NO_ANNOTATE(NonWartTokenIdEl, NonWartTokenId, token_id)          \
    NO_ANNOTATE(NonceIdEl, NonceId, nonce_id)                        \
    NO_ANNOTATE(NonceReservedEl, NonceReserved, nonce_reserved)      \
    NO_ANNOTATE(OrderIdEl, HistoryId, order_id)                      \
    NO_ANNOTATE(OriginAccIdEl, AccountId, origin_account_id)         \
    NO_ANNOTATE(PinHeightEl, PinHeight, pin_height)                  \
    NO_ANNOTATE(PinNonceEl, PinNonce, pin_nonce)                     \
    NO_ANNOTATE(QuoteEl, Wart, quote)                                \
    NO_ANNOTATE(QuoteWartEl, Wart, quote_wart)                       \
    NO_ANNOTATE(ReferredHistoryIdEl, HistoryId, referred_history_id) \
    NO_ANNOTATE(SharesEl, Funds_uint64, shares)                      \
    NO_ANNOTATE(SignatureEl, RecoverableSignature, signature)        \
    NO_ANNOTATE(ToAccIdEl, AccountId, to_id)                         \
    NO_ANNOTATE(ToAddrEl, Address, to_addr)                          \
    ANNOTATE(AddrEl, Address, address, "address")                    \
    NO_ANNOTATE(WartEl, Wart, wart)

#define ELEMENT_DEFINE_NOANNOTATE(structname, datatype, methodname)   \
    struct structname : public ElementBase<datatype> {                \
        using ElementBase::ElementBase;                               \
        [[nodiscard]] const auto& methodname() const { return data; } \
    };
#define ELEMENT_DEFINE_ANNOTATE(structname, datatype, methodname, annotation)             \
    struct structname : public ElementBaseWithAnnotation<annotation, datatype> {          \
        using ElementBaseWithAnnotation<annotation, datatype>::ElementBaseWithAnnotation; \
        [[nodiscard]] const auto& methodname() const { return data; }                     \
    };
ELEMENTMAP(ELEMENT_DEFINE_NOANNOTATE, ELEMENT_DEFINE_ANNOTATE)
#undef ERR_DEFINE

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

struct BuyEl : BoolElBase {
    using BoolElBase::BoolElBase;
    bool buy() const { return get(); }
};

struct TransactionIdEl : public ElementBase<TransactionId> {
    using ElementBase::ElementBase;
    [[nodiscard]] const TransactionId& txid() const { return data; }
    [[nodiscard]] AccountId from_id() const { return txid().accountId; }
    [[nodiscard]] PinHeight pin_height() const { return txid().pinHeight; }
    [[nodiscard]] NonceId nonce_id() const { return txid().nonceId; }
};
