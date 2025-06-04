#pragma once
#include "block/block.hpp"
struct ChainMiningTask {
    block::Block block;
};
struct BlockWorker {
    block::Block block;
    std::string worker;
};
