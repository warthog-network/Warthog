#pragma once
#include "block/body/body.hpp"
#include "block/chain/height.hpp"
#include "block/header/header.hpp"

struct TransactionId;

namespace block {
struct Block {
    NonzeroHeight height;
    Header header;
    std::vector<uint8_t> bodyData;
    Body body;
    Block(NonzeroHeight height, std::span<const uint8_t, 80> header, std::vector<uint8_t> body);
};
}
