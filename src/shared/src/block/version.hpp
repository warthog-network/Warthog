#pragma once
#include "general/with_uint64.hpp"
class BlockVersion : public IsUint32 {
public:
    explicit constexpr BlockVersion(uint32_t v)
        : IsUint32(v)
    {
    }

    static const BlockVersion v3;
    static const BlockVersion v4;

    auto operator<=>(uint32_t v)
    {
        return value() <=> (v);
    }
    auto operator==(uint32_t v)
    {
        return value() == v;
    }
};
inline constexpr const BlockVersion BlockVersion::v3 { 3 };
inline constexpr const BlockVersion BlockVersion::v4 { 4 };
