#pragma once
#include "block/chain/height.hpp"
#include "general/descriptor.hpp"

struct BlockRange {
    BlockRange(NonzeroHeight lower, NonzeroHeight upper)
        : lower(lower)
        , upper(upper)
    {
        assert(valid());
    }

    // data
    NonzeroHeight lower;
    NonzeroHeight upper;
    uint32_t length() const { return upper - lower + 1; }
    BlockRange(Reader&);
    friend Writer& operator<<(Writer&, BlockRange);

private:
    bool valid();
};

struct DescriptedBlockRange:public BlockRange {
    Descriptor descriptor;
    DescriptedBlockRange(Descriptor descriptor, NonzeroHeight lowerHeight, NonzeroHeight upperHeight)
        : BlockRange{lowerHeight, upperHeight},
            descriptor(descriptor) {}
    DescriptedBlockRange(Reader& r);
    friend Writer& operator<<(Writer&, DescriptedBlockRange);
};
