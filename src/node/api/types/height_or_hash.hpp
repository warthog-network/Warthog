#pragma once
#include<variant>
#include"block/chain/height.hpp"
#include"crypto/hash.hpp"

namespace API {
struct HeightOrHash{
    std::variant<Height,Hash> data;
};
}
