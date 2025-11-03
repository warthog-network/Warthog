#include "block.hpp"
#include "block/header/header_impl.hpp"
namespace block {

BlockData::BlockData(NonzeroHeight height, Header header, BodyData body)
    : height(std::move(height))
    , header(std::move(header))
    , body(std::move(body), this->header.version())
{
}
Block BlockData::parse_throw(ParseAnnotations* annotations) &&
{
    return { height, std::move(header), Body::parse_throw(std::move(body), height, annotations) };
}
}
