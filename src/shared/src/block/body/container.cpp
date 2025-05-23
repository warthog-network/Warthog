#include "container.hpp"
#include "block/body/view.hpp"
#include "general/errors.hpp"
#include "general/params.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"

namespace block {
namespace body {
Container::Container(std::span<const uint8_t> s)
    : bytes(s.begin(), s.end())
{
    if (s.size() > MAXBLOCKSIZE) {
        throw Error(EBLOCKSIZE);
    }
}

Structure Container::parse_structure_throw(NonzeroHeight h, BlockVersion v) const
{
    return Structure::parse_throw(bytes, h, v);
}

Container::Container(Reader& r)
{
    auto s { r.span() };
    bytes.assign(s.begin(), s.end());
}

Writer& operator<<(Writer& r, const Container& b)
{
    return r << (uint32_t)b.bytes.size() << Range(b.bytes);
}
}
}
