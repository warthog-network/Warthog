#include "container.hpp"
#include "block/body/body.hpp"
#include "block/chain/height.hpp"
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

std::pair<ParsedBody, body::MerkleLeaves> BodyData::parse(NonzeroHeight h, BlockVersion version) const
{
    body::MerkleLeaves l;
    auto body { Body::parse_throw(*this, h, version, &l) };
    return { std::move(body), std::move(l) };
}

VersionedBodyData BodyData::make_versioned(BlockVersion v) &&
{
    return { std::move(*this), v };
}
}
