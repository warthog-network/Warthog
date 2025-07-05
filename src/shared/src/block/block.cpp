#include "block.hpp"
#include "block/header/header_impl.hpp"

namespace block {
Block::Block(NonzeroHeight height, HeaderView header, BodyContainer bodyData)
    : height(height)
    , header(header)
    , bodyData(std::move(bodyData))
    , body(this->bodyData, this->header.version(), height)
{
}
}
