#pragma once
#include "block/chain/height.hpp"
#include "eventloop/types/conref_declaration.hpp"
#include "general/errors.hpp"
struct ChainOffender : public ChainError {
    ChainOffender(const ChainError& ce, Conref cr)
        : ChainError(ce)
        , conId(cr.id())
    {
    }
    ChainOffender(const ChainError& ce, uint64_t connectionId)
        : ChainError(ce)
        , conId(connectionId)
    {
    }
    ChainOffender(int32_t e, NonzeroHeight height, uint64_t connectionId)
        : ChainError(e, height)
        , conId(connectionId) {};
    uint64_t conId;
};
