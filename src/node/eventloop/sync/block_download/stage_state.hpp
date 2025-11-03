#pragma once
#include "block/chain/height.hpp"
#include "block/chain/offender.hpp"
#include "communication/stage_operation/result.hpp"
#include "connection_data.hpp"
#include <variant>

struct ChainError;
namespace BlockDownload {

struct BanEntry {
    Height forkHeight;
    uint64_t connId;
    BanEntry(Height fh, uint64_t cid)
        : forkHeight(fh)
        , connId(cid)
    {
    }
};

struct PendingStageOperation {
    struct StageSetType {
        Height length;
    };
    struct StageAddType {
        std::vector<BanEntry> banMemory;
    };

    void set_stage_add(const ForkIter& begin, const ForkIter& end);
    void set_stage_set(Height length);
    void set_nothing()
    {
        var = {};
    }

    operator bool()
    {
        return busy();
    }
    bool busy()
    {
        return !std::holds_alternative<std::monostate>(var);
    }
    bool is_stage_set()
    {
        return std::holds_alternative<StageSetType>(var);
    }
    bool is_stage_add()
    {
        return std::holds_alternative<StageAddType>(var);
    }
    auto& stage_set_data()
    {
        return std::get<StageSetType>(var);
    }

    [[nodiscard]] auto pop_add_data()
    {
        auto d { std::move(std::get<StageAddType>(var)) };
        set_nothing();
        return d;
    }
    [[nodiscard]] auto pop_set_data()
    {
        auto d { std::move(std::get<StageSetType>(var)) };
        set_nothing();
        return d;
    }
    void clear()
    {
        *this = {};
    }

private: // data
    std::variant<std::monostate, StageSetType, StageAddType> var;
};

struct StageState {
    // data
    Height stageSetAck { 0 };
    wrt::optional<Height> staleFrom;
    PendingStageOperation pendingOperation;

    // methods
    [[nodiscard]] bool is_stage_set_phase() const
    {
        return stageSetPhase;
    }
    void set_stale_from(Height from);

    void clear();
    void clear_non_pending();
    [[nodiscard]] std::vector<ChainOffender> on_result(const stage_operation::StageAddStatus&);
    [[nodiscard]] wrt::optional<Height> on_result(const stage_operation::StageSetStatus&);

private:
    bool stageSetPhase { true };
};
}
