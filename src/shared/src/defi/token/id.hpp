#pragma once
#include "general/with_uint64.hpp"
struct TokenId : public IsUint32 {
    using IsUint32::IsUint32;

    static const TokenId WART;
    bool operator==(const TokenId&) const = default;
    auto operator-(TokenId a)
    {
        return val - a.val;
    }
    TokenId operator-(uint32_t i) const
    {
        return TokenId(val - i);
    }
    TokenId operator+(uint32_t i) const
    {
        return TokenId(val + i);
    }
    TokenId operator++(int)
    {
        return TokenId(val++);
    }
};
inline constexpr TokenId TokenId::WART{0};

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
