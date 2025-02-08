#include "range.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
bool BlockRange::valid()
{
    return _lower <= _upper
        && (_upper - _lower + 1 <= MAXBLOCKBATCHSIZE);
}

Writer& operator<<(Writer& w, BlockRange br)
{
    return w << br._lower << br._upper;
}

BlockRange::BlockRange(Reader& r)
    : _lower(r)
    , _upper(r)
{
    if (!valid())
        throw Error(EBLOCKRANGE);
}

DescriptedBlockRange::DescriptedBlockRange(Reader& r)
    : BlockRange(r)
    , descriptor(r) {}

Writer& operator<<(Writer& w, DescriptedBlockRange dbr)
{
    return w << *static_cast<BlockRange*>(&dbr) << dbr.descriptor;
}
