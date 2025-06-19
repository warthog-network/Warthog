#pragma once
#include "general/with_uint64.hpp"
struct TokenId : public UInt32WithOperators<TokenId> {
    using UInt32WithOperators::UInt32WithOperators;
    static const TokenId WART;
};

struct ShareId : public TokenId {
    ShareId(TokenId id)
        : TokenId(std::move(id))
    {
    }
    using TokenId::TokenId;
};

inline constexpr TokenId TokenId::WART { 0 };

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
