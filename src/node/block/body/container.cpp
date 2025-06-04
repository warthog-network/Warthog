#include "container.hpp"
#include "block/body/body.hpp"
#include "block/chain/height.hpp"
#include "tools/span.hpp"

namespace block {

BodyContainer::BodyContainer(Reader& r)
    : data(to_vector(r.span_4()))
{
}

VersionedBodyContainer::VersionedBodyContainer(BodyContainer bc, BlockVersion v)
    : BodyContainer(std::move(bc))
    , version(v)
{
}

block::Body BodyContainer::parse(NonzeroHeight h, BlockVersion version) const
{
    return block::Body::parse_throw(data, h, version);
}

VersionedBodyContainer BodyContainer::make_versioned(BlockVersion v) &&
{
    return { std::move(*this), v };
}
}
