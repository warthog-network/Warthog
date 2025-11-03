#pragma once

#include "block/chain/height.hpp"
namespace chainserver {
namespace state_update {
    struct StageUpdate {
        wrt::optional<Height> shrinkLength;
    };
}
}
