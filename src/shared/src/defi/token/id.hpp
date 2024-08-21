#include "general/with_uint64.hpp"
struct TokenId : public IsUint32 {
    using IsUint32::IsUint32;

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
