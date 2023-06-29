#pragma once

#include "block/header/header.hpp"
#include "block/chain/height.hpp"
#include "block/body/container.hpp"
#include <vector>

struct Block {
    NonzeroHeight height;
    Header header;
    BodyContainer body;
    bool operator==(const Block&)const=default;
    operator bool() { return body.size() > 0; }
};
