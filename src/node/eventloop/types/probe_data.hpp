#pragma once
#include "block/chain/fork_range.hpp"
#include "block/chain/header_chain.hpp"

struct ProbeData {
    ProbeData(ForkRange fr, Headerchain::pin_t headers)
        : _forkRange(fr)
        , _headers(std::move(headers))
    {
    }
    void match(NonzeroHeight h, HeaderView hv)
    {
        _forkRange.match(*_headers, h, hv);
    }
    auto& fork_range() const { return _forkRange; }
    auto& headers() const { return _headers; }

private:
    ForkRange _forkRange;
    Headerchain::pin_t _headers;
};
