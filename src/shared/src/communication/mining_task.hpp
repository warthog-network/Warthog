#pragma once

#include "block/block.hpp"

struct ChainMiningTask {
    Block block;
};
struct BlockWorker {
    Block block;
    std::string worker;
};
