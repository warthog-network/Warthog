#include "apply_stage.hpp"
#include "block/header/header_impl.hpp"
#include "block_applier.hpp"
#include "general/hex.hpp"
#include "general/now.hpp"
#include <fstream>

namespace chainserver {
ApplyStageTransaction::ApplyStageTransaction(const State& s, ChainDBTransaction&& transaction)
    : ccs(s)
    , transaction(std::move(transaction))
    , chainlength(s.chainlength())
{
}

[[nodiscard]] ChainError ApplyStageTransaction::apply_stage_blocks()
{
    assert(!applyResult);
    applyResult = AppendBlocksResult {};
    auto& res { applyResult.value() };
    auto& baseTxIds { rb ? rb->chainTxIds : ccs.chainstate.txids() };
    chainserver::BlockApplier ba { ccs.db, ccs.stage, baseTxIds, true };
    for (NonzeroHeight h = (chainlength + 1).nonzero_assert(); h <= ccs.stage.length(); ++h) {
        auto historyId { ccs.db.next_history_id() };
        AccountId accountId { ccs.db.next_account_id() };
        auto hash { ccs.stage.hash_at(h) };
        auto p = ccs.db.get_block(hash);
        if (!p) {
            throw std::runtime_error("Bug at line " + std::to_string(__LINE__)
                + ". Cannot get block with hash " + serialize_hex(hash)
                + " at height " + std::to_string(h) + " from database.");
        }
        auto [blockId, block] = *p;
        assert(block.height == h);

        try {
            auto apiBlock { ba.apply_block(block, hash, blockId) };
            apiBlocks.push_back(std::move(apiBlock));
        } catch (Error e) {
            std::string fname { std::to_string(now_timestamp())
                + "_" + std::to_string(h.value())
                + "_" + std::string(e.err_name())
                + "_failed.block" };
            std::ofstream f(fname);
            f << serialize_hex(block.header) << '\n'
              << serialize_hex(block.bodyData);
            res.newTxIds = ba.move_new_txids();
            return { e, h };
        }
        res.newHistoryOffsets.push_back(historyId);
        res.newAccountOffsets.push_back(accountId);
        chainlength = h;
    }
    res.newTxIds = ba.move_new_txids();
    res.wartUpdates = ba.move_balance_updates();
    return { Error(0), (ccs.stage.length() + 1).nonzero_assert() };
}

void ApplyStageTransaction::consider_rollback(Height shrinkLength)
{
    assert(!rb);
    assert(shrinkLength <= ccs.chainlength());
    if (shrinkLength < ccs.chainlength()) {
        rb = ccs.rollback(shrinkLength);
        chainlength = rb->shrink.length;
    }
    assert(chainlength == shrinkLength);
}

auto ApplyStageTransaction::commit(State& cs) && -> commit_t
{
    assert(!commited);
    assert(applyResult);
    commited = true;

    std::unique_lock<std::mutex> ul(cs.chainstateMutex);
    auto result { rb ? cs.commit_fork(std::move(*rb), std::move(*applyResult))
                     : cs.commit_append(std::move(*applyResult)) };
    transaction.commit();
    return {
        .update { std::move(result) },
        .appendedBlocks = std::move(apiBlocks)
    };
}
}
