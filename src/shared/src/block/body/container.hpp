#pragma once
#include "block/body/body_fwd.hpp"
#include "block/chain/height_fwd.hpp"
#include "block/version.hpp"
#include <cstdint>
namespace block {

struct VersionedBodyContainer;
struct BodyContainer: public std::vector<uint8_t> {
    BodyContainer(Reader& r);
    BodyContainer(std::vector<uint8_t> v)
        : vector(v)
    {
    }
    VersionedBodyContainer make_versioned(BlockVersion v) &&;
    block::Body parse(NonzeroHeight, BlockVersion) const;
};

struct VersionedBodyContainer : public BodyContainer {
    VersionedBodyContainer(BodyContainer bc, BlockVersion v);
    BlockVersion version;
};

}
