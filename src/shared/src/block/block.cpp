#include "block.hpp"
#include "block/header/header_impl.hpp"

namespace block {
Block::Block(NonzeroHeight height, std::span<const uint8_t, 80> header, std::vector<uint8_t> bodyData)
    : height(height)
    , header(header)
    , bodyData(std::move(bodyData))
    , body(this->bodyData, this->header.version(), height)
{
}
}
