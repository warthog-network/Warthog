#pragma once
#include "block/chain/height.hpp"
#include "general/descriptor.hpp"

struct HeightRange {
    HeightRange(NonzeroHeight lower, NonzeroHeight upper)
        : _lower(lower)
        , _upper(upper)
    {
        assert(upper > lower);
    }

    // data
    NonzeroHeight lower() const { return _lower; }
    NonzeroHeight upper() const { return _upper; }
    NonzeroHeight end() const { return _upper + 1; }
    uint32_t length() const { return _upper - _lower + 1; }
    friend Writer& operator<<(Writer&, HeightRange);

private:
    NonzeroHeight _lower;
    NonzeroHeight _upper;
};

template <size_t limit, Error error>
struct HeightRangeLimited : public HeightRange {
    HeightRangeLimited(NonzeroHeight lower, NonzeroHeight upper)
        : HeightRange(lower, upper)
    {
        assert(valid());
    }
    HeightRangeLimited(Reader& r)
        : HeightRangeLimited { NonzeroHeight(r), NonzeroHeight(r) }
    {
        if (!valid())
            throw error;
    }

private:
    bool valid()
    {
        return lower() <= upper()
            && (upper() - lower() + 1 <= limit);
    }
};

using BlockRange = HeightRangeLimited<MAXBLOCKSIZE, Error(EBLOCKRANGE)>;
using HeaderRange = HeightRangeLimited<HEADERBATCHSIZE, Error(EHEADERRANGE)>;

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
