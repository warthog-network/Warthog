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

template <typename... Elements>
struct CombineElements : public Elements... {
    using combined_t = CombineElements<Elements...>;
    CombineElements(Reader& r)
        : Elements(r)...
    {
    }
    [[nodiscard]] size_t byte_size() const { return (Elements::byte_size() + ...); }
    CombineElements(Elements::data_t... ts)
        : Elements(std::move(ts))...
    {
    }
    template <typename Element>
    auto& get() const
    {
        return static_cast<const Element*>(this)->get();
    }
    friend Writer& operator<<(Writer& w, const CombineElements& e)
    {
        return (w << ... << static_cast<const Elements*>(e)->get());
    }
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
    using base = ElementBase;
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
    using base = ElementBase;
    using data_t = T;

protected:
    T data;
};

struct AccountIdEl : public ElementBase<AccountId> {
    using base::base;
    [[nodiscard]] const AccountId& account_id() const { return data; }
};
struct ToAccIdEl : public ElementBase<AccountId> {
    using base::base;
    [[nodiscard]] const AccountId& to_id() const { return data; }
};
struct OriginAccIdEl : public ElementBase<AccountId> {
    using base::base;
    [[nodiscard]] const AccountId& origin_account_id() const { return data; }
};
struct OwnerIdEl : public ElementBase<AccountId> {
    using base::base;
    [[nodiscard]] const AccountId& owner_account_id() const { return data; }
};

struct TokenSupplyEl : public ElementBase<FundsDecimal> {
    using base::base;
    [[nodiscard]] const auto& supply() const { return data; }
};
struct ToAddrEl : public ElementBase<Address> {
    using base::base;
    [[nodiscard]] const auto& to_addr() const { return data; }
};
struct CreatorAddrEl : public ElementBase<Address> {
    using base::base;
    [[nodiscard]] const auto& creator_addr() const { return data; }
};

struct WartEl : public ElementBase<Wart> {
    using base::base;
    [[nodiscard]] const Wart& wart() const { return data; }
};

struct QuoteWartEl : public ElementBase<Wart> {
    using base::base;
    [[nodiscard]] const Wart& quote_wart() const { return data; }
};
struct AmountEl : public ElementBase<Funds_uint64> {
    using base::base;
    [[nodiscard]] const Funds_uint64& amount() const { return data; }
};
struct BaseAmountEl : public ElementBase<Funds_uint64> {
    using base::base;
    [[nodiscard]] const Funds_uint64& base_amount() const { return data; }
};
struct CancelPinNonceEl : public ElementBase<PinNonce> {
    using base::base;
    [[nodiscard]] const PinNonce& block_pin_nonce() const { return data; }
};
struct CancelTxidEl : public ElementBase<TransactionId> {
    using base::base;
    [[nodiscard]] const auto& cancel_pin_nonce() const { return data; }
    [[nodiscard]] const auto& cancel_account_id() const { return data.accountId; }
};
struct OrderIdEl : public ElementBase<HistoryId> {
    using base::base;
    [[nodiscard]] const auto& order_id() const { return data; }
};
struct ReferredHistoryIdEl : public ElementBase<HistoryId> {
    using base::base;
    [[nodiscard]] const auto& referred_history_id() const { return data; }
};
struct PinNonceEl : public ElementBase<PinNonce> {
    using base::base;
    [[nodiscard]] const PinNonce& pin_nonce() const { return data; }
};
struct CompactFeeEl : public ElementBase<CompactUInt> {
    using base::base;
    [[nodiscard]] const CompactUInt& compact_fee() const { return data; }
    [[nodiscard]] Wart fee() const { return data.uncompact(); }
};
struct TokenPrecisionEl : public ElementBase<TokenPrecision> {
    using base::base;
    [[nodiscard]] const TokenPrecision& token_precision() const { return data; }
};
struct TokenNameEl : public ElementBase<TokenName> {
    using base::base;
    [[nodiscard]] const TokenName& token_name() const { return data; }
};
struct TokenHashEl : public ElementBase<TokenHash> {
    using base::base;
    [[nodiscard]] const auto& token_hash() const { return data; }
};
struct TokenIdEl : public ElementBase<TokenId> {
    using base::base;
    [[nodiscard]] const auto& token_id() const { return data; }
};
struct SignatureEl : public ElementBase<RecoverableSignature> {
    using base::base;
    [[nodiscard]] const RecoverableSignature& signature() const { return data; }
};
struct LimitPriceEl : public ElementBase<Price_uint64> {
    using base::base;
    [[nodiscard]] const Price_uint64& limit() const { return data; }
};
struct BuyEl {
    BuyEl(uint8_t v)
        : b(v != 0)
    {
        if (v > 1)
            throw Error(EINVBUY);
    }
    using data_t = bool;
    const bool& get() const { return b; }
    static constexpr size_t byte_size() { return 1; }
    bool buy() const { return b; }

private:
    bool b;
};

struct PinHeightEl : public ElementBase<PinHeight> {
    using base::base;
    [[nodiscard]] const PinHeight& pin_height() const { return data; }
};

struct TransactionIdEl : public ElementBase<TransactionId> {
    using base::base;
    [[nodiscard]] const TransactionId& txid() const { return data; }
    [[nodiscard]] AccountId from_id() const { return txid().accountId; }
    [[nodiscard]] PinHeight pin_height() const { return txid().pinHeight; }
    [[nodiscard]] NonceId nonce_id() const { return txid().nonceId; }
};

struct NonceIdEl : public ElementBase<NonceId> {
    using base::base;
    [[nodiscard]] const NonceId& nonce_id() const { return data; }
};

struct NonceReservedEl : public ElementBase<NonceReserved> {
    using base::base;
    [[nodiscard]] const NonceReserved& nonce_reserved() const { return data; }
};

struct BaseEl : public ElementBase<Funds_uint64> {
    using base::base;
    [[nodiscard]] const auto& base() const { return data; }
};

struct QuoteEl : public ElementBase<Wart> {
    using base::base;
    [[nodiscard]] const auto& quote() const { return data; }
};
