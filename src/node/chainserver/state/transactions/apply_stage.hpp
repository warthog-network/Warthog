#pragma once
#include "../state.hpp"
#include "api/types/forward_declarations.hpp"
#include "chainserver/db/chain_db.hpp"

namespace chainserver {
class ApplyStageTransaction {
    using StateUpdate = state_update::StateUpdate;
    using commit_t = state_update::StateUpdateWithAPIBlocks;

public:
    ApplyStageTransaction(const State& s, ChainDBTransaction&& transaction);

    void consider_rollback(Height shrinkLength);
    [[nodiscard]] ChainError apply_stage_blocks();
    [[nodiscard]] commit_t commit(State&) &&;

private:
    const State& ccs; // const ref
    ChainDBTransaction transaction;
    Height chainlength;
    std::optional<RollbackResult> rb;
    std::optional<AppendBlocksResult> applyResult;
    std::vector<api::Block> apiBlocks;

    bool commited = false;
};

}
