#pragma once
#include<variant>
#include"block/chain/height.hpp"
#include"crypto/hash.hpp"

namespace api {
struct HeightOrHash{
    std::variant<Height,Hash> data;
};
}
