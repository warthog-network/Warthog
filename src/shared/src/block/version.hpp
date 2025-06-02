#pragma once
#include "general/with_uint64.hpp"
class BlockVersion : public IsUint32 {
public:
    constexpr BlockVersion(uint32_t v)
        : IsUint32(v)
    {
    }
    auto operator<=>(uint32_t v)
    {
        return value() <=> (v);
    }
};
