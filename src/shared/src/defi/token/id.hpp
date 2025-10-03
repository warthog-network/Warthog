#pragma once
#include "general/errors.hpp"
#include "general/with_uint64.hpp"
#include <cassert>
#include <optional>

struct AssetId;
class NonWartTokenId;
struct TokenId : public UInt64WithOperators<TokenId> {
    using UInt64WithOperators::UInt64WithOperators;
    static const TokenId WART;
    [[nodiscard]] bool is_wart() const { return value() == 0; }
    [[nodiscard]] bool is_share() const
    {
        return (value() & 1) != 0; // shares have odd ids
    }
    [[nodiscard]] std::optional<NonWartTokenId> non_wart() const;
};

class NonWartTokenId : public TokenId {
    friend struct AssetId;

public:
    [[nodiscard]] AssetId asset_id() const;

    static std::optional<NonWartTokenId> non_wart(TokenId id)
    {
        if (id.is_wart())
            return {};
        return NonWartTokenId(id);
    }

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

inline std::optional<NonWartTokenId> TokenId::non_wart() const
{
    return NonWartTokenId::non_wart(*this);
}

struct AssetId : public UInt64WithOperators<AssetId> { // assets are tokens that are not pool shares
    using UInt64WithOperators<AssetId>::UInt64WithOperators;
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
