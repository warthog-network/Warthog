#pragma once

#include "block/block.hpp"

struct ChainMiningTask {
    ParsedBlock block;
};
struct BlockWorker {
    Block block;
    std::string worker;
};
