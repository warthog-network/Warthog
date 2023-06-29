#pragma once
#include<compare>
#include "general/with_uint64.hpp"
class BlockId :public IsUint64{
public:
    explicit BlockId(int64_t val)
        : IsUint64(val)
    {
    }
    bool operator==(const BlockId&) const = default;
    auto operator<=>(const BlockId&) const = default;
};
