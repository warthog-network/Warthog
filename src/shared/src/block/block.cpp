#include "block.hpp"
#include "block/header/header_impl.hpp"
namespace block {
Block::Block(NonzeroHeight height, HeaderView header, BodyData body)
    : Block(height, header, Body::parse_throw(VersionedBodyData(body, header.version()), height))
{
}
}
