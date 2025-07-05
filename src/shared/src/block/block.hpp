#pragma once
#include "block/body/body.hpp"
#include "block/body/container.hpp"
#include "block/chain/height.hpp"
#include "block/header/header.hpp"
#include "block_fwd.hpp"

struct TransactionId;

namespace block {
struct Block {
    NonzeroHeight height;
    Header header;
    BodyContainer bodyData;
    Body body;
    auto tx_ids() const { return body.tx_ids(height); }
    Block(NonzeroHeight height, HeaderView header, BodyContainer body);
};

struct BlockWithHash : public Block {
    BlockHash hash;
};
}
