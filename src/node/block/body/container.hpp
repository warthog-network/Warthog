#pragma once
#include "block/body/body_fwd.hpp"
#include "block/chain/height_fwd.hpp"
#include "block/version.hpp"
#include <cstdint>
namespace block {

struct VersionedBodyContainer;
struct BodyContainer {
    BodyContainer(Reader& r);
    BodyContainer(std::vector<uint8_t> v)
        : data(v)
    {
    }
    VersionedBodyContainer make_versioned(BlockVersion v) &&;
    block::Body parse(NonzeroHeight, BlockVersion) const;
    std::vector<uint8_t> data;
};

struct VersionedBodyContainer : public BodyContainer {
    VersionedBodyContainer(BodyContainer bc, BlockVersion v);
    BlockVersion version;
};

}
