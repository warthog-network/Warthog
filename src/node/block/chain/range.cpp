#include "range.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"


Writer& operator<<(Writer& w, NonemptyHeightRange br)
{
    return w << br.first() << br.last();
}

DescriptedBlockRange::DescriptedBlockRange(Reader& r)
    : BlockRange(r)
    , descriptor(r) {}

Writer& operator<<(Writer& w, DescriptedBlockRange dbr)
{
    return w << *static_cast<BlockRange*>(&dbr) << dbr.descriptor;
}
