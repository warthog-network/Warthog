#pragma once
#include "block/chain/batch_slot.hpp"
#include "block/header/view.hpp"
struct ChainPin {
    Height height;
    HeaderView header;
};
struct GridPin {
    Batchslot slot;
    HeaderView finalHeader;
    static wrt::optional<GridPin> checkpoint();
};
