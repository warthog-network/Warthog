#pragma once
#include<cstdint>
#include<cassert>
#include "general/with_uint64.hpp"
struct DeletionKey :IsUint64{
    DeletionKey(uint64_t i):IsUint64(i){
        assert(i!=0);
    }
    DeletionKey operator++(int){
        return val++;
    }
};
