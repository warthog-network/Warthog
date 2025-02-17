#pragma once
#include "block/chain/worksum.hpp"
#include "block/header/header.hpp"
struct HeightHeaderWork : public HeightHeader {
    Worksum worksum;
    HeightHeaderWork(NonzeroHeight height, Header header, Worksum worksum)
        : HeightHeader(height, std::move(header))
        , worksum(std::move(worksum))
    {
    }
    bool operator==(const HeightHeaderWork&) const = default;
};

struct RogueHeaderData : public HeightHeaderWork {
    RogueHeaderData(ChainError ce, Header header, Worksum worksum)
        : HeightHeaderWork(ce.height(), header, worksum)
        , error(ce)
    {
    }
    ChainError chain_error() const { return { error, height }; }
    Error error;
};
