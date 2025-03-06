#pragma once
#include "general/with_uint64.hpp"

class OrderId : public IsUint64 { // NOTE: >=0 is important for OrderId
public:
    using IsUint64::IsUint64;
    bool operator==(const OrderId&) const = default;
    size_t operator-(OrderId a)
    {
        return val - a.val;
    }
    OrderId operator-(size_t i) const
    {
        return OrderId(val - i);
    }
    OrderId operator+(size_t i) const
    {
        return OrderId(val + i);
    }
    OrderId operator++(int)
    {
        return OrderId(val++);
    }
};
