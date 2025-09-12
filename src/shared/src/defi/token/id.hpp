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
    [[nodiscard]] std::optional<AssetId> asset_id() const;
};

class NonWartTokenId : public TokenId {
    friend struct AssetId;

public:
    [[nodiscard]] AssetId asset_id() const;

private:
    constexpr NonWartTokenId(TokenId tid)
        : TokenId(std::move(tid))
    {
    }
public:
    NonWartTokenId(Reader& r)
        : TokenId(r)
    {
        if (is_wart())
            throw Error(EWARTTOKID);
    }
};

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

    constexpr NonWartTokenId token_id(bool poolLiquidity = false) const { return TokenId { 1 + 2 * value() + poolLiquidity }; }
};

inline constexpr TokenId TokenId::WART { 0 };

inline AssetId NonWartTokenId::asset_id() const
{
    return AssetId((value() - 1) >> 1);
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
