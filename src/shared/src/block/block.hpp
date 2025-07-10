#pragma once
#include "block/body/body.hpp"
#include "block/header/header.hpp"
#include "block_fwd.hpp"

namespace block {
struct Block {
    NonzeroHeight height;
    Header header;
    Body body;
    auto tx_ids() const { return body.tx_ids(height); }
    Block(NonzeroHeight height, HeaderView header, BodyData body);
    Block(NonzeroHeight height, HeaderView header, Body body)
        : height(std::move(height))
        , header(std::move(header))
        , body(std::move(body))
    {
    }
};

}
