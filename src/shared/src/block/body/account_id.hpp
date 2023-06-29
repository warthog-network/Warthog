#pragma once
#include "general/with_uint64.hpp"
#include <compare>
#include <cstddef>
#include <cstdint>

class AccountId : public IsUint64 {// NOTE: >=0 is important for AccountHeights
public:
    using IsUint64::IsUint64;
    bool operator==(const AccountId&) const = default;
    size_t operator-(AccountId a)
    {
        return val - a.val;
    }
    AccountId operator-(size_t i) const{
        return AccountId(val - i);
    }
    AccountId operator+(size_t i) const
    {
        return AccountId(val + i);
    }
    AccountId operator++(int)
    {
        return AccountId(val++);
    }
};
