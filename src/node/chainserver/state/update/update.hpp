#pragma once
#include "chainstate_update.hpp"
#include "mempool_update.hpp"
#include "api/types/all.hpp"
#include "stage_update.hpp"

namespace chainserver {
namespace state_update {

    struct StateUpdate {
        ChainstateUpdate chainstateUpdate;
        MempoolUpdate mempoolUpdate;
    };
    struct StateUpdateWithAPIBlocks {
        StateUpdate update;
        std::vector<api::Block> appendedBlocks;
    };

}
}
