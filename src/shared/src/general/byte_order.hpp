#pragma once
#include <bit>
#include <climits>
#include <cstdint>
static_assert(CHAR_BIT == 8);

constexpr inline uint64_t byte_swap64(uint64_t x)
{
    return (((x & 0xff00000000000000ull) >> 56)
        | ((x & 0x00ff000000000000ull) >> 40)
        | ((x & 0x0000ff0000000000ull) >> 24)
        | ((x & 0x000000ff00000000ull) >> 8)
        | ((x & 0x00000000ff000000ull) << 8)
        | ((x & 0x0000000000ff0000ull) << 24)
        | ((x & 0x000000000000ff00ull) << 40)
        | ((x & 0x00000000000000ffull) << 56));
}

constexpr inline uint32_t byte_swap32(uint32_t x)
{
    return (((x & 0xff000000U) >> 24) | ((x & 0x00ff0000U) >> 8) | ((x & 0x0000ff00U) << 8) | ((x & 0x000000ffU) << 24));
}

constexpr inline uint16_t byte_swap16(uint16_t x)
{
    return (x >> 8) | (x << 8);
}


constexpr inline uint16_t hton16(uint16_t x)
{
    if constexpr (std::endian::native != std::endian::big) {
        return byte_swap16(x);
    }
    return x;
}

constexpr inline uint16_t ntoh16(uint16_t x)
{
    return hton16(x);
}
constexpr inline uint32_t hton32(uint32_t x)
{
    if constexpr (std::endian::native != std::endian::big) {
        return byte_swap32(x);
    }
    return x;
}
constexpr inline uint32_t ntoh32(uint32_t x)
{
    return hton32(x);
}

constexpr inline uint64_t hton64(uint64_t x)
{
    if constexpr (std::endian::native != std::endian::big) 
        return byte_swap64(x);
    return x;
}
constexpr inline uint64_t ntoh64(uint64_t x)
{
    return hton64(x);
}
