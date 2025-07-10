#include "block.hpp"
#include "block/header/header_impl.hpp"
namespace block {
Block::Block(NonzeroHeight height, HeaderView header, BodyData body)
    : Block(height, header, Body::parse({ std::move(body), header.version() }, height))
{
}
}
