#pragma once
#include "general/with_uint64.hpp"

class Reader;
struct Descriptor : public IsUint32 {
    Descriptor(uint32_t i)
        : IsUint32(i) { };
    Descriptor(Reader&);
    Descriptor operator+(uint32_t i) const
    {
        return { value() + i };
    }
};
