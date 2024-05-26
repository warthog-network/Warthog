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
    static constexpr size_t byte_size(){return NonzeroHeight::byte_size()*2;}
    BlockRange(Reader&);
    friend Writer& operator<<(Writer&, BlockRange);

private:
    bool valid();
};

struct DescriptedBlockRange:public BlockRange {
    Descriptor descriptor;
    static constexpr size_t byte_size(){return BlockRange::byte_size() + Descriptor::byte_size();}
    DescriptedBlockRange(Descriptor descriptor, NonzeroHeight lowerHeight, NonzeroHeight upperHeight)
        : BlockRange{lowerHeight, upperHeight},
            descriptor(descriptor) {}
    DescriptedBlockRange(Reader& r);
    friend Writer& operator<<(Writer&, DescriptedBlockRange);
};
