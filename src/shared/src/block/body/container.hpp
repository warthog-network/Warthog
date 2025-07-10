#pragma once
#include "block/body/body_fwd.hpp"
#include "block/chain/height_fwd.hpp"
#include "block/version.hpp"
#include <cstdint>
namespace block {

struct VersionedBodyData;
struct BodyData : public std::vector<uint8_t> {
    BodyData(Reader& r);
    BodyData(std::vector<uint8_t> v)
        : vector(v)
    {
    }
    size_t byte_size() const { return size(); }
    VersionedBodyData make_versioned(BlockVersion v) &&;
    [[nodiscard]] std::pair<ParsedBody, body::MerkleLeaves> parse(NonzeroHeight, BlockVersion) const;
};

struct VersionedBodyData : public BodyData {
    VersionedBodyData(BodyData bc, BlockVersion v);
    BlockVersion version;
};

}
