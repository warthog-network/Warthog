#pragma once
#include "block/chain/fork_range.hpp"
#include "block/chain/header_chain.hpp"

struct ProbeData {
    ForkRange forkRange;
    Headerchain::pin_t headers;
    void match(NonzeroHeight h, HeaderView hv)
    {
        forkRange.match(*headers, h, hv);
    }
};
