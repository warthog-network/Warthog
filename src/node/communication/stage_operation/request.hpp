#pragma once

#include "block/block.hpp"
#include "block/chain/header_chain.hpp"
#include <variant>
namespace stage_operation {

struct StageSetOperation {
    Headerchain headers;
};

struct StageAddOperation {
    Headerchain headers;// LATER: remove if no more bugs
    std::vector<ParsedBlock> blocks;
};

using Operation = std::variant<StageSetOperation, StageAddOperation>;

}
