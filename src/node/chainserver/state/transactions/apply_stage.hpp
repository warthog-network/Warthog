#pragma once
#include "../state.hpp"
#include "chainserver/db/chain_db.hpp"
#include "api/types/forward_declarations.hpp"

namespace chainserver {
class ApplyStageTransaction {
    using StateUpdate = state_update::StateUpdate;
public:
    ApplyStageTransaction(const State& s, ChainDBTransaction&& transaction);

    void consider_rollback(Height shrinkLength);
    [[nodiscard]] std::pair<std::vector<API::Block>,ChainError> apply_stage_blocks();
    [[nodiscard]] StateUpdate commit(State&);

private:
    const State& ccs; // const ref
    ChainDBTransaction transaction;
    Height chainlength;
    std::optional<RollbackResult> rb;
    std::optional<AppendBlocksResult> applyResult;

    bool commited = false;
};

}
