#pragma once
#include "base_elements_fwd.hpp"
#include "block/body/account_id.hpp"
#include "block/body/nonce.hpp"
#include "block/chain/height.hpp"
#include "crypto/crypto.hpp"
#include "defi/token/token.hpp"
#include "defi/uint64/price.hpp"
#include "general/compact_uint.hpp"
#include "general/funds.hpp"

template <typename T>
struct ElementBase {
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

struct ToIdElement : public ElementBase<AccountId> {
    using base::base;
    [[nodiscard]] const AccountId& to_id() const { return data; }
};
struct ToAddrElement : public ElementBase<Address> {
    using base::base;
    [[nodiscard]] const auto& to_addr() const { return data; }
};

struct WartElement : public ElementBase<Wart> {
    using base::base;
    [[nodiscard]] const Wart& wart() const { return data; }
};
struct AmountElement : public ElementBase<Funds_uint64> {
    using base::base;
    [[nodiscard]] const Funds_uint64& amount() const { return data; }
};
struct OriginAccountIdElement : public ElementBase<AccountId> {
    using base::base;
    [[nodiscard]] const AccountId& origin_account_id() const { return data; }
};
struct CancelPinNonceElement : public ElementBase<PinNonce> {
    using base::base;
    [[nodiscard]] const PinNonce& block_pin_nonce() const { return data; }
};
struct PinNonceElement : public ElementBase<PinNonce> {
    using base::base;
    [[nodiscard]] const PinNonce& pin_nonce() const { return data; }
};
struct CompactFeeElement : public ElementBase<CompactUInt> {
    using base::base;
    [[nodiscard]] const CompactUInt& compact_fee() const { return data; }
    [[nodiscard]] Wart fee() const { return data.uncompact(); }
};
struct TokenSupplyElement : public ElementBase<Funds_uint64> {
    using base::base;
    [[nodiscard]] const Funds_uint64& token_supply() const { return data; }
};
struct TokenPrecisionElement : public ElementBase<TokenPrecision> {
    using base::base;
    [[nodiscard]] const TokenPrecision& token_precision() const { return data; }
};
struct TokenNameElement : public ElementBase<TokenName> {
    using base::base;
    [[nodiscard]] const TokenName& token_name() const { return data; }
};
struct TokenHashElement : public ElementBase<TokenHash> {
    using base::base;
    [[nodiscard]] const auto& token_hash() const { return data; }
};
struct SignatureElement : public ElementBase<RecoverableSignature> {
    using base::base;
    [[nodiscard]] const RecoverableSignature& signature() const { return data; }
};
struct LimitPriceElement : public ElementBase<Price_uint64> {
    using base::base;
    [[nodiscard]] const Price_uint64& limit() const { return data; }
};
struct BuyElement {
    BuyElement(uint8_t v)
        : b(v != 0)
    {
        if (v > 1)
            throw Error(EINVBUY);
    }
    static constexpr size_t byte_size() { return 1; }
    bool buy() const { return b; }

private:
    bool b;
};

struct PinHeightElement : public ElementBase<PinHeight> {
    using base::base;
    [[nodiscard]] const PinHeight& pin_height() const { return data; }
};

struct NonceIdElement : public ElementBase<NonceId> {
    using base::base;
    [[nodiscard]] const NonceId& nonce_id() const { return data; }
};

struct NonceReservedElement : public ElementBase<NonceReserved> {
    using base::base;
    [[nodiscard]] const NonceReserved& nonce_reserved() const { return data; }
};

template <typename... Elements>
struct CombineElements : public Elements... {
    using combined_t = CombineElements<Elements...>;
    CombineElements(Reader& r)
        : Elements(r)...
    {
    }
    static constexpr size_t byte_size() { return (Elements::byte_size() + ...); }
    CombineElements(Elements::data_t... ts)
        : Elements(std::move(ts))...
    {
    }
};
