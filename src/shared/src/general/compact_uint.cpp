#include "compact_uint.hpp"
#include "general/reader.hpp"
CompactUInt::CompactUInt(Reader& r)
    :CompactUInt(from_value_throw(r.uint16()))
{
}
CompactUInt CompactUInt::compact(Wart f, bool ceil)
{
    if (f.is_zero())
        return uint16_t(0x0000u);
    uint16_t e = 10;
    const uint64_t threshold = uint64_t(0x07FFu);
    uint64_t e8 { f.E8() };
    bool exact { true };
    while (e8 > threshold) {
        e += 1;
        if (ceil && ((e8 & 1) != 0)) {
            exact = false;
        }
        e8 >>= 1;
    }
    if (ceil && exact == false) {
        e8 += 1;
        if (e8 > threshold) {
            e8 >>= 1;
            e += 1;
            if (e > 53)
                return largest();
        }
    }
    while (e8 < uint64_t(0x0400u)) {
        e -= 1;
        e8 <<= 1;
    }
    return (e << 10) | (uint16_t(e8) & uint16_t(0x03FF));
}
