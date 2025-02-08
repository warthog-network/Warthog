#include "range.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"

Writer& operator<<(Writer& w, HeightRange br)
{
    return w << br._lower << br._upper;
}


DescriptedBlockRange::DescriptedBlockRange(Reader& r)
    : BlockRange(r)
    , descriptor(r) {}

Writer& operator<<(Writer& w, DescriptedBlockRange dbr)
{
    return w << *static_cast<BlockRange*>(&dbr) << dbr.descriptor;
}
