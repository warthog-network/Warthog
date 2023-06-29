#pragma once
#include "crypto/hash.hpp"
#include "difficulty_declaration.hpp"
#include "general/params.hpp"
#include <arpa/inet.h>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

// Target encoding (4 bytes):
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
// constexpr uint32_t EASIESTTARGET_HOST=0x00FFFFFFu; // no zeros and all 24 digits set to 1
constexpr uint32_t HARDESTTARGET_HOST = 0xe8800000u; // maximal target, 232 zeros then one digit 1 and 23 digits 0
constexpr uint32_t GENESISTARGET_HOST = (uint32_t(GENESISDIFFICULTYEXPONENT) << 24) | 0x00FFFFFFu;
static_assert(GENESISDIFFICULTYEXPONENT < 0xe8u);

inline Target::Target(double difficulty)
    : Target(0u)
{
    if (difficulty < 1.0)
        difficulty = 1.0;
    int exp;
    double coef = std::frexp(difficulty, &exp);
    double inv = 1 / coef; // will be in the interval (1,2]
    if (exp - 1 >= 256 - 24) {
        data = htonl(HARDESTTARGET_HOST);
        return;
    };
    at(0) = uint8_t(exp - 1);
    if (inv == 2.0) {
        at(1) = 0xffu;
        at(2) = 0xffu;
        at(3) = 0xffu;
    } else [[likely]] { // need to shift by 23 to the left
        uint32_t digits(std::ldexp(inv, 23));
        if (digits < 0x00800000u)
            digits = 0x00800000u;
        if (digits > 0x00ffffffu)
            digits = 0x00ffffffu;
        at(3) = digits & 0xffu;
        digits >>= 8;
        at(2) = digits & 0xffu;
        digits >>= 8;
        at(1) = digits & 0xffu;
    }
}

inline uint32_t Target::bits() const
{ // returns values in [2^23,2^24)
  //(uint32_t)(at(1))<<16 |(uint32_t)(at(2))<<8 | (uint32_t)(at(3));
    return 0x00FFFFFFul & ntohl(data);
}

inline bool Target::compatible(const Hash& hash) const
{
    uint8_t zeros = at(0);
    if (zeros > (256u - 4 * 8u))
        return false;
    if ((at(1) & 0x80) == 0)
        return false; // first digit must be 1
    const size_t zerobytes = zeros / 8; // number of complete zero bytes
    const size_t shift = zeros & 0x07u;

    for (size_t i = 0; i < zerobytes; ++i)
        if (hash[31 - i] != 0u)
            return false; // here we need zeros

    uint32_t threshold = bits() << (8u - shift);
    uint32_t candidate;
    uint8_t* dst = reinterpret_cast<uint8_t*>(&candidate);
    const uint8_t* src = &hash[28 - zerobytes];
    dst[0] = src[3];
    dst[1] = src[2];
    dst[2] = src[1];
    dst[3] = src[0];
    candidate = ntohl(candidate);
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
inline void Target::scale(uint32_t easierfactor, uint32_t harderfactor)
{
    assert(easierfactor != 0);
    assert(harderfactor != 0);
    if (easierfactor >= 0x80000000u)
        easierfactor = 0x7FFFFFFFu; // prevent overflow
    if (harderfactor >= 0x80000000u)
        harderfactor = 0x7FFFFFFFu; // prevent overflow
    int zeros = at(0);
    uint64_t bits64 = bits();
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
    at(3) = bits64 & 0xffu;
    bits64 >>= 8;
    at(2) = bits64 & 0xffu;
    bits64 >>= 8;
    at(1) = bits64 & 0xffu;
checks:
    if (zeros < GENESISDIFFICULTYEXPONENT) {
        data = htonl(GENESISTARGET_HOST);
        return;
    }
    if (zeros > 232) { // 232=256-3*8
        data = htonl(HARDESTTARGET_HOST);
        return;
    }
    at(0) = zeros;
}
inline double Target::difficulty() const
{
    const int zeros = at(0);
    double dbits = bits();
    return std::ldexp(1 / dbits, zeros + 24); // first digit  of ((uint8_t*)(&encodedDifficulty))[1] is 1, compensate for other 23 digts of the 3 byte mantissa
}
inline Target Target::genesis()
{
    return htonl(GENESISTARGET_HOST);
}
