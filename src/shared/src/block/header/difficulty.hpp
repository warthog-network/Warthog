#pragma once
#include "block/header/hash_exponential_request.hpp"
#include "crypto/hash.hpp"
#include "difficulty_declaration.hpp"
#include "general/byte_order.hpp"
#include "general/params.hpp"
// #include <arpa/inet.h>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

// TargetV1 encoding (4 bytes):
//
// byte 0:   number of required zeros,
// byte 1-3: 24 base 2 digits starting at position [byte 0] from left
// Note: maximum is 256-32=224 because more difficult targets won't be
//       necessary is most likely not in this case the bits with index
//       224-255
//
// The following constants are defined in terms of host uint32_t numbers for convenience
// and need to be converted to big endian (network byte order) to match the byte
// ordering required above
//

constexpr TargetV1::TargetV1(uint32_t data)
    : data(hton32(data)) {};

inline TargetV1::TargetV1(double difficulty)
{
    if (difficulty < 1.0)
        difficulty = 1.0;
    int exp;
    double coef = std::frexp(difficulty, &exp);
    double inv = 1 / coef; // will be in the interval (1,2]
    if (exp - 1 >= 256 - 24) {
        data = hton32(HARDESTTARGET_HOST);
        return;
    };
    uint32_t zeros = exp - 1;
    if (inv == 2.0) {
        set(zeros,0x00FFFFFFu);
    } else [[likely]] { // need to shift by 23 to the left
        uint32_t digits(std::ldexp(inv, 23));
        if (digits < 0x00800000u)
            set(zeros,0x00800000u);
        else if (digits > 0x00ffffffu)
            set(zeros,0x00FFFFFFu);
        else
            set(zeros,digits);
    }
}

inline uint32_t TargetV1::zeros8() const
{
    return data >> 24;
};

inline uint32_t TargetV1::bits24() const
{ // returns values in [2^23,2^24)
    return 0x00FFFFFFul & data;
}

[[nodiscard]] inline bool TargetV1::compatible(const Hash& hash) const
{
    auto zeros = zeros8();
    if (zeros > (256u - 4 * 8u))
        return false;
    uint32_t bits = bits24();
    if ((bits & 0x00800000u) == 0)
        return false; // first digit must be 1
    const size_t zerobytes = zeros / 8; // number of complete zero bytes
    const size_t shift = zeros & 0x07u;

    for (size_t i = 0; i < zerobytes; ++i)
        if (hash[31 - i] != 0u)
            return false; // here we need zeros

    uint32_t threshold = bits << (8u - shift);
    uint32_t candidate;
    uint8_t* dst = reinterpret_cast<uint8_t*>(&candidate);
    const uint8_t* src = &hash[28 - zerobytes];
    dst[0] = src[3];
    dst[1] = src[2];
    dst[2] = src[1];
    dst[3] = src[0];
    candidate = ntoh32(candidate);
    if (candidate > threshold) {
        return false;
    }
    if (candidate < threshold) [[likely]] {
        return true;
    }
    for (size_t i = 0; i < 28 - zerobytes; ++i)
        if (hash[i] != 0)
            return false;
    return true;
}
inline void TargetV1::scale(uint32_t easierfactor, uint32_t harderfactor)
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
    set(zeros,bits64);
}
inline double TargetV1::difficulty() const
{
    const int zeros = zeros8();
    double dbits = bits24();
    return std::ldexp(1 / dbits, zeros + 24); 
}
inline TargetV1 TargetV1::genesis()
{
    return hton32(GENESISTARGET_HOST);
}

// TargetV2 encoding (4 bytes):
//
// byte 0:   number of required zeros,
// byte 1-3: 24 base 2 digits starting at position [byte 0] from left
// Note: maximum is 256-32=224 because more difficult targets won't be
//       necessary is most likely not in this case the bits with index
//       224-255
//
// The following constants are defined in terms of host uint32_t numbers for convenience
// and need to be converted to big endian (network byte order) to match the byte
// ordering required above
//

constexpr TargetV2::TargetV2(uint32_t data)
    : data(hton32(data)) {};

inline TargetV2::TargetV2(double difficulty)
    : TargetV2(0u)
{
    if (difficulty < 1.0)
        difficulty = 1.0;
    int exp;
    double coef = std::frexp(difficulty, &exp);
    double inv = 1 / coef; // will be in the interval (1,2]
    uint32_t zeros = exp - 1;
    if (zeros >= 3 * 256) {
        data = hton32(MaxTargetHost);
        return;
    };
    if (inv == 2.0) {
        set(zeros, 0x003fffffu);
    } else [[likely]] { // need to shift by 21 to the left
        uint32_t digits(std::ldexp(inv, 21));
        if (digits < 0x00200000u)
            set(zeros, 0x00200000u);
        else if (digits > 0x003fffffu)
            set(zeros, 0x003fffffu);
        else
            set(zeros, digits);
    }
}

inline uint32_t TargetV2::bits22()
    const
{ // returns values in [2^21,2^22)
    return 0x003FFFFFul & data;
}

inline uint32_t TargetV2::zeros10() const
{
    return data >> 22;
};

inline void TargetV2::scale(uint32_t easierfactor, uint32_t harderfactor)
{
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
    if (zeros < MinDiffExponent) {
        data = MinTargetHost;
        return;
    }
    if (zeros >= 256 * 3) {
        data = MaxTargetHost;
        return;
    }
    set(zeros, bits64);
}

inline double TargetV2::difficulty() const
{
    const int zeros = zeros10();
    double dbits = bits22();
    return std::ldexp(
        1 / dbits,
        zeros + 22); // first digit  of ((uint8_t*)(&encodedDifficulty))[1] is 1,
                     // compensate for other 23 digts of the 3 byte mantissa
}
inline TargetV2 TargetV2::min()
{
    return hton32(MinTargetHost);
}

inline bool TargetV2::compatible(const HashExponentialDigest& digest) const
{
    auto zerosTarget { zeros10() };
    assert(digest.negExp > 0);
    auto zerosDigest { digest.negExp - 1 };
    if (zerosTarget < zerosDigest)
        return true;
    if (zerosTarget > zerosDigest)
        return false;
    auto bits32 { bits22() << 10 };
    return digest.data < bits32;
}

