#pragma once
#include "block/chain/height.hpp"
#include "general/errors.hpp"
#include <optional>
#include <variant>
namespace stage_operation {

struct StageSetResult {
    std::optional<NonzeroHeight> firstMissHeight;
};
struct StageAddResult {
    ChainError err;
};
using Result = std::variant<StageSetResult, StageAddResult>;
}
