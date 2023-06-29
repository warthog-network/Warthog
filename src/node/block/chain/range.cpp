#include "range.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
bool BlockRange::valid()
{
    return lower <= upper
        && (upper - lower + 1 <= MAXBLOCKBATCHSIZE);
}

Writer& operator<<(Writer& w, BlockRange br)
{
    return w << br.lower << br.upper;
};

BlockRange::BlockRange(Reader& r)
    : lower(r)
    , upper(r)
{
    if (!valid())
        throw Error(EBLOCKRANGE);
}

DescriptedBlockRange::DescriptedBlockRange(Reader& r)
    : BlockRange(r)
    , descriptor(r) {};

Writer& operator<<(Writer& w, DescriptedBlockRange dbr)
{
    return w << *static_cast<BlockRange*>(&dbr) << dbr.descriptor;
};
