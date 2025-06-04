#pragma once

#include "block/block_fwd.hpp"
#include "block/chain/header_chain.hpp"
#include <variant>
namespace stage_operation {

struct StageSetOperation {
    Headerchain headers;
};

struct StageAddOperation {
    Headerchain headers;// LATER: remove if no more bugs
    std::vector<Block> blocks;
};

using Operation = std::variant<StageSetOperation, StageAddOperation>;

}
