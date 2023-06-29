#pragma once

#include "block/chain/height.hpp"
namespace chainserver {
namespace state_update {
    struct StageUpdate {
        std::optional<Height> shrinkLength;
    };
}
}
