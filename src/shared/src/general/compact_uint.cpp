#include "compact_uint.hpp"
#include "general/writer.hpp"
Writer& operator<<(Writer& w, CompactUInt cf){
    return w<<cf.value();
};
CompactUInt CompactUInt::compact(Funds f){
    if (f.is_zero())
        return uint16_t(0x0000u);
    uint16_t e = 10;
    const uint64_t threshold = uint64_t(0x07FFu);
    uint64_t e8{f.E8()};
    while (e8 > threshold) {
        e += 1;
        e8 >>= 1;
    }
    while (e8 < uint64_t(0x0400u)) {
        e -= 1;
        e8 <<= 1;
    }
    return (e << 10) | (uint16_t(e8) & uint16_t(0x03FF));
}
