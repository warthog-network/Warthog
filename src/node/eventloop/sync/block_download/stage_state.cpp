#include "stage_state.hpp"
#include "eventloop/types/conndata.hpp"
#include "general/errors.hpp"

namespace BlockDownload {
void PendingStageOperation::set_stage_add(const ForkIter& begin, const ForkIter& end)
{
    assert(!busy());

    std::vector<BanEntry> banMemory;
    for (auto iter = begin; iter != end; ++iter) {
        banMemory.push_back(BanEntry(iter->first, iter->second.id()));
    }
    var = StageAddType { std::move(banMemory) };
}

void PendingStageOperation::set_stage_set(Height length)
{
    assert(!busy());
    var = StageSetType { length };
}

void StageState::clear()
{
    if (pendingOperation.busy()) {
        staleFrom = Height(1);
    } else {
        clear_non_pending();
    }
}
void StageState::clear_non_pending()
{
    assert(!pendingOperation.busy());
    *this = {};
}
void StageState::set_stale_from(Height from)
{
    if (!staleFrom.has_value() || from < *staleFrom) {
        staleFrom = from;
    }
}

std::vector<ChainOffender> StageState::on_result(const stage_operation::StageAddStatus& r)
{
    auto data { pendingOperation.pop_add_data() };
    auto& e = r.ce;
    stageSetAck = e.height() - 1;
    std::vector<ChainOffender> offenders;
    if (e) {
        if (e.code != ELEADERMISMATCH) { // is peer's fault
            for (auto& p : data.banMemory) {
                if (p.forkHeight > e.height()) {
                    offenders.push_back({ e, p.connId });
                }
            }
        }
    }
    if (staleFrom.has_value() && *staleFrom < e.height())
        clear_non_pending();
    return offenders;
}
wrt::optional<Height> StageState::on_result(const stage_operation::StageSetStatus& e)
{
    auto data { pendingOperation.pop_set_data() };
    if (!e.firstMissHeight || (staleFrom.has_value() && *staleFrom < *e.firstMissHeight)) {
        clear_non_pending();
        return {};
    }
    stageSetAck = *e.firstMissHeight - 1;
    Height firstMissHeight { *e.firstMissHeight };
    if (firstMissHeight <= data.length)
        stageSetPhase = false;
    return firstMissHeight;
}
}
