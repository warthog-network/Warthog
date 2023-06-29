#pragma once

#include<cstdint>
#include<limits>
inline uint64_t to_unonce(int64_t nonce){
    return nonce;// let the C++ standard do the correct conversion
}

inline int64_t to_nonce(uint64_t unonce){
    constexpr uint64_t threshold=std::numeric_limits<int64_t>::max();
    if (unonce > threshold) return -(int64_t)((-unonce)-1)-1;
    return unonce;
}
