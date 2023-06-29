#pragma once
#include "general/with_uint64.hpp"

struct Descriptor : public IsUint32 {
    Descriptor(uint32_t i):IsUint32(i){};
    Descriptor operator+(uint32_t i) const{
        return {value()+i};
    }
};
