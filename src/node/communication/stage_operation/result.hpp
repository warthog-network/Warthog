#pragma once
#include "block/chain/height.hpp"
#include "general/errors.hpp"
#include <optional>
#include <variant>
namespace stage_operation {

struct StageSetStatus {
    std::optional<NonzeroHeight> firstMissHeight;
};
struct StageAddStatus {
    ChainError ce;
};
using Result = std::variant<StageSetStatus, StageAddStatus>;
}
