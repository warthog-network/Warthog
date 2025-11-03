#pragma once
#include "block/chain/height.hpp"
#include "general/errors.hpp"
#include "wrt/optional.hpp"
#include <variant>
namespace stage_operation {

struct StageSetStatus {
    wrt::optional<NonzeroHeight> firstMissHeight;
};
struct StageAddStatus {
    ChainError ce;
};
using Result = std::variant<StageSetStatus, StageAddStatus>;
}
