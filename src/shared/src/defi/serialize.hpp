#pragma once

#include "defi/uint64/price.hpp"
#include "general/writer.hpp"
inline Writer& operator<<(Writer& w, const Price_uint64& p){
    return w<<p.to_uint32();
};
