#include "block/header/difficulty_declaration.hpp"
#include "global/globals.hpp"
inline void TargetV1::scale(uint32_t easierfactor, uint32_t harderfactor, Height)
{
    assert(easierfactor != 0);
    assert(harderfactor != 0);
    if (easierfactor >= 0x80000000u)
        easierfactor = 0x7FFFFFFFu; // prevent overflow
    if (harderfactor >= 0x80000000u)
        harderfactor = 0x7FFFFFFFu; // prevent overflow
    int zeros = zeros8();
    uint64_t bits64 = bits24();
    if (harderfactor >= 2 * easierfactor) { // cap like in bitcoin but here with 2 instead of 4
        zeros += 1;
        goto checks;
    }
    if (easierfactor >= 2 * harderfactor) {
        zeros -= 1;
        goto checks;
    }
    if (harderfactor > easierfactor) { // target shall increase in difficulty
        easierfactor <<= 1;
        zeros += 1;
    }
    bits64 = (bits64 * uint64_t(easierfactor)) / uint64_t(harderfactor);
    if (bits64 > 0x00FFFFFFul) {
        bits64 >>= 1;
        zeros -= 1;
    }
    assert(bits64 <= 0x00FFFFFFul);
    assert(bits64 & 0x00800000ul);
checks:
    if (zeros < GENESISDIFFICULTYEXPONENT) {
        data = GENESISTARGET_HOST;
        return;
    }
    if (zeros >= 255) {
        data = HARDESTTARGET_HOST;
        return;
    }
    set(zeros, bits64);
}

inline void TargetV2::scale(uint32_t easierfactor, uint32_t harderfactor, Height height)
{
    bool v2 { height.value() > JANUSV2RETARGETSTART };
    // uint8_t MinDiffExponent = (v2? 40 : 43);
    // Target MinTargetHost( (MinDiffExponent << 22) | 0x003FFFFFu);
    auto MinTargetHost = (v2? initialv2() : initial());
    if (is_testnet()){
        MinTargetHost = genesis_testnet();
    }

    assert(easierfactor != 0);
    assert(harderfactor != 0);
    if (easierfactor >= 0x80000000u)
        easierfactor = 0x7FFFFFFFu; // prevent overflow
    if (harderfactor >= 0x80000000u)
        harderfactor = 0x7FFFFFFFu; // prevent overflow
    uint32_t zeros = zeros10();
    uint64_t bits64 = bits22();
    if (harderfactor >= 2 * easierfactor) { // cap like in bitcoin but here with 2 instead of 4
        zeros += 1;
        goto checks;
    }
    if (easierfactor >= 2 * harderfactor) {
        zeros -= 1;
        goto checks;
    }
    if (harderfactor > easierfactor) { // target shall increase in difficulty
        easierfactor <<= 1;
        zeros += 1;
    }
    bits64 = (bits64 * uint64_t(easierfactor)) / uint64_t(harderfactor);
    if (bits64 > 0x003FFFFFul) {
        bits64 >>= 1;
        zeros -= 1;
    }
    assert(bits64 <= 0x003FFFFFul);
    assert((bits64 & 0x00200000ul) != 0);
checks:
    if (zeros < MinTargetHost.zeros10()) {
        *this = MinTargetHost;
        return;
    }
    if (zeros >= 256 * 3) {
        data = MaxTargetHost;
        return;
    }
    set(zeros, bits64);
}
