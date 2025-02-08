#pragma once
#include "block/chain/height.hpp"
#include "general/descriptor.hpp"

struct BlockRange {
    BlockRange(NonzeroHeight lower, NonzeroHeight upper)
        : _lower(lower)
        , _upper(upper)
    {
        assert(valid());
    }

    // data
    NonzeroHeight lower() const { return _lower; }
    NonzeroHeight upper() const { return _upper; }
    NonzeroHeight end() const { return _upper + 1; }
    uint32_t length() const { return _upper - _lower + 1; }
    BlockRange(Reader&);
    friend Writer& operator<<(Writer&, BlockRange);

private:
    NonzeroHeight _lower;
    NonzeroHeight _upper;
    bool valid();
};

struct DescriptedBlockRange : public BlockRange {
    Descriptor descriptor;
    DescriptedBlockRange(Descriptor descriptor, NonzeroHeight lowerHeight, NonzeroHeight upperHeight)
        : BlockRange { lowerHeight, upperHeight }
        , descriptor(descriptor)
    {
    }
    DescriptedBlockRange(Reader& r);
    friend Writer& operator<<(Writer&, DescriptedBlockRange);
};
