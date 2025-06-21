#pragma once
#include "general/with_uint64.hpp"
#include <cassert>
#include <optional>

struct AssetId;
struct TokenId : public UInt32WithOperators<TokenId> {
    using UInt32WithOperators::UInt32WithOperators;
    static const TokenId WART;
    std::optional<AssetId> as_asset() const;
    AssetId corresponding_asset_id() const;
};

struct ShareId;
struct AssetId : public UInt32WithOperators<AssetId> { // assets are tokens that are not pool shares
    static const AssetId WART;
    constexpr explicit AssetId(uint32_t id)
        : UInt32WithOperators(id)
    {
    }
    AssetId(Reader& r)
        : UInt32WithOperators<AssetId>(r)
    {
    }

    constexpr TokenId token_id() const { return TokenId { 2 * value() }; }
    ShareId share_id() const;
};
inline constexpr AssetId AssetId::WART { 0 };
inline constexpr TokenId TokenId::WART { AssetId::WART.token_id() };

struct ShareId : public UInt32WithIncrement<ShareId> { // shares are tokens that specify pool participation for an asset
    explicit ShareId(uint64_t id)
        : UInt32WithIncrement(id)
    {
    }
    TokenId token_id() const { return TokenId { 2 * value() + 1 }; }
    AssetId asset_id() const { return AssetId { value() }; }
};

inline std::optional<AssetId> TokenId::as_asset() const
{
    if ((value() & 1) != 0) // if odd
        return {};
    return AssetId(value() >> 1);
}

inline AssetId TokenId::corresponding_asset_id() const{
    return AssetId(value() >> 1);
}

inline ShareId AssetId::share_id() const
{
    return ShareId(value());
}

struct TokenForkId : public IsUint64 {
    using IsUint64::IsUint64;

    static const TokenForkId WART;
    bool operator==(const TokenForkId&) const = default;
    auto operator-(TokenForkId a)
    {
        return val - a.val;
    }
    TokenForkId operator-(uint64_t i) const
    {
        return TokenForkId(val - i);
    }
    TokenForkId operator+(uint64_t i) const
    {
        return TokenForkId(val + i);
    }
    TokenForkId operator++(int)
    {
        return TokenForkId(val++);
    }
};

struct TokenForkBalanceId : public IsUint64 {
    using IsUint64::IsUint64;

    static const TokenForkBalanceId WART;
    bool operator==(const TokenForkBalanceId&) const = default;
    auto operator-(TokenForkBalanceId a)
    {
        return val - a.val;
    }
    TokenForkBalanceId operator-(uint64_t i) const
    {
        return TokenForkBalanceId(val - i);
    }
    TokenForkBalanceId operator+(uint64_t i) const
    {
        return TokenForkBalanceId(val + i);
    }
    TokenForkBalanceId operator++(int)
    {
        return TokenForkBalanceId(val++);
    }
};
