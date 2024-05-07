#include "descriptor.hpp"
#include "general/reader.hpp"

Descriptor::Descriptor(Reader& r)
    :Descriptor(r.uint32())
{
}
