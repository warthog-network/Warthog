#pragma once
#include "general/with_uint64.hpp"
class BlockId : public IsUint64 {
public:
    explicit BlockId(int64_t val)
        : IsUint64(val)
    {
    }
};
