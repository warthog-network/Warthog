#include "container.hpp"
#include "block/body/body.hpp"
#include "block/chain/height.hpp"
#include "general/reader.hpp"
#include "tools/span.hpp"

namespace block {

BodyData::BodyData(Reader& r)
    : std::vector<uint8_t>(to_vector(r.span_4()))
{
}

VersionedBodyData::VersionedBodyData(BodyData bc, BlockVersion v)
    : BodyData(std::move(bc))
    , version(v)
{
}

Body BodyData::parse_throw(NonzeroHeight h, BlockVersion version) &&
{
    return Body::parse_throw({ std::move(*this), version }, h);
}

VersionedBodyData BodyData::make_versioned(BlockVersion v) &&
{
    return { std::move(*this), v };
}
}
