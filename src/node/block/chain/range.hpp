#pragma once
#include "block/chain/height.hpp"
#include "general/descriptor.hpp"

struct NonemptyHeightRange : public HeightRange {
protected:
    NonemptyHeightRange(NonzeroHeight hbegin, NonzeroHeight hend)
        : HeightRange(hbegin, hend)
    {
        assert(hbegin < hend);
    }
public:
    static constexpr size_t byte_size() { return 8; }
    friend Writer& operator<<(Writer&, HeightRange);
};

template <size_t limit, Error error>
struct HeightRangeLimited : public NonemptyHeightRange {
    HeightRangeLimited(NonzeroHeight hbegin, NonzeroHeight hend)
        : NonemptyHeightRange(hbegin, hend)
    {
        assert(valid());
    }
    HeightRangeLimited(Reader& r)
        : HeightRangeLimited { NonzeroHeight(r), NonzeroHeight(r) + 1 } // end is past last element
    {
        if (!valid())
            throw error;
    }

private:
    bool valid()
    {
        return first() <= last()
            && length() <= limit;
    }
};

using BlockRange = HeightRangeLimited<MAXBLOCKSIZE, Error(EBLOCKRANGE)>;
using HeaderRange = HeightRangeLimited<HEADERBATCHSIZE, Error(EHEADERRANGE)>;

struct DescriptedBlockRange : public BlockRange {
    Descriptor descriptor;
    static constexpr size_t byte_size() { return BlockRange::byte_size() + Descriptor::byte_size(); }
    DescriptedBlockRange(Descriptor descriptor, BlockRange br)
        : BlockRange { std::move(br) }
        , descriptor(descriptor)
    {
    }
    DescriptedBlockRange(Reader& r);
    friend Writer& operator<<(Writer&, DescriptedBlockRange);
};
