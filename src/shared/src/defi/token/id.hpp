#pragma once
#include "general/errors.hpp"
#include "general/with_uint64.hpp"
#include <cassert>
#include <optional>

struct AssetId;
struct TokenId : public UInt32WithOperators<TokenId> {
    using UInt32WithOperators::UInt32WithOperators;
    static const TokenId WART;
    bool is_wart() const { return value() == 0; }
    bool is_share() const
    {
        return (value() & 1) != 0; // shares have odd ids
    }
    [[nodiscard]] std::optional<AssetId> as_asset() const;
    [[nodiscard]] AssetId corresponding_asset_id() const;
};

class NonWartTokenId : public TokenId {
    friend struct AssetId;

private:
    constexpr NonWartTokenId(TokenId tid)
        : TokenId(std::move(tid))
    {
    }
    NonWartTokenId(Reader& r)
        : TokenId(r)
    {
        if (is_wart())
            throw Error(EWARTTOKID);
    }
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

    ShareId share_id() const;
    constexpr NonWartTokenId token_id(bool liquidityDerivative = false) const { return TokenId { 1 + 2 * value() + liquidityDerivative }; }
};

inline constexpr TokenId TokenId::WART { 0 };

struct ShareId : public UInt32WithIncrement<ShareId> { // shares are tokens that specify pool participation for an asset
    explicit ShareId(uint64_t id)
        : UInt32WithIncrement(id)
    {
    }
    AssetId asset_id() const { return AssetId { value() }; }
    TokenId token_id() const { return asset_id().token_id(true); }
};

inline AssetId TokenId::corresponding_asset_id() const
{
    return AssetId(value() >> 1);
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
