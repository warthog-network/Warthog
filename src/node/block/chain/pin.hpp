#pragma once
#include "block/chain/batch_slot.hpp"
#include "block/header/view.hpp"
#include "crypto/hash.hpp"
struct ChainPin {
    Height height;
    Hash hash;
};
struct GridPin {
    Batchslot slot;
    HashView finalHeader;
    static std::optional<GridPin> checkpoint();
};
