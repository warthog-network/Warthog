#pragma once
#include "block/chain/height.hpp"
#include "general/errors.hpp"
#include "chainserver/state/update/update.hpp"
#include <optional>
#include <variant>
namespace stage_operation {

struct StageSetResult {
    std::optional<NonzeroHeight> firstMissHeight;
};
struct StageAddResult {
    ChainError ce;
};
using Result = std::variant<StageSetResult, StageAddResult>;
}
