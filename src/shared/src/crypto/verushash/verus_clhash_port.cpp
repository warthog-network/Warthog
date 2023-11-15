#include "verus_clhash_port.hpp"

#include "crypto/hash.hpp"
#include "verushash.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __APPLE__
#include <sys/types.h>
#endif // APPLE

#ifdef __linux__

#if defined(__i386__) || defined(__X86_64__)
#include <x86intrin.h>
#elif defined(__arm__) || defined(__aarch64__)
#include "crypto/sse2neon.h"
#endif

#elif _WIN32
#pragma warning(disable : 4146)
#include <intrin.h>
#endif

namespace{
static void aesenc(unsigned char *s, const unsigned char *rk);

#define AES2_EMU(s0, s1, rci)                                                  \
  aesenc((unsigned char *)&s0, (unsigned char *)&(rc[rci]));                   \
  aesenc((unsigned char *)&s1, (unsigned char *)&(rc[rci + 1]));               \
  aesenc((unsigned char *)&s0, (unsigned char *)&(rc[rci + 2]));               \
  aesenc((unsigned char *)&s1, (unsigned char *)&(rc[rci + 3]));

static inline __m128i _mm_unpacklo_epi32_emu(__m128i a, __m128i b) {
  uint32_t result[4];
  uint32_t *tmp1 = (uint32_t *)&a, *tmp2 = (uint32_t *)&b;
  result[0] = tmp1[0];
  result[1] = tmp2[0];
  result[2] = tmp1[1];
  result[3] = tmp2[1];
  return *(__m128i *)result;
}

static inline __m128i _mm_unpackhi_epi32_emu(__m128i a, __m128i b) {
  uint32_t result[4];
  uint32_t *tmp1 = (uint32_t *)&a, *tmp2 = (uint32_t *)&b;
  result[0] = tmp1[2];
  result[1] = tmp2[2];
  result[2] = tmp1[3];
  result[3] = tmp2[3];
  return *(__m128i *)result;
}

#define MIX2_EMU(s0, s1)                                                       \
  tmp = _mm_unpacklo_epi32_emu(s0, s1);                                        \
  s1 = _mm_unpackhi_epi32_emu(s0, s1);                                         \
  s0 = tmp;

#define saes_data(w)                                                           \
  {                                                                            \
    w(0x63), w(0x7c), w(0x77), w(0x7b), w(0xf2), w(0x6b), w(0x6f), w(0xc5),    \
        w(0x30), w(0x01), w(0x67), w(0x2b), w(0xfe), w(0xd7), w(0xab),         \
        w(0x76), w(0xca), w(0x82), w(0xc9), w(0x7d), w(0xfa), w(0x59),         \
        w(0x47), w(0xf0), w(0xad), w(0xd4), w(0xa2), w(0xaf), w(0x9c),         \
        w(0xa4), w(0x72), w(0xc0), w(0xb7), w(0xfd), w(0x93), w(0x26),         \
        w(0x36), w(0x3f), w(0xf7), w(0xcc), w(0x34), w(0xa5), w(0xe5),         \
        w(0xf1), w(0x71), w(0xd8), w(0x31), w(0x15), w(0x04), w(0xc7),         \
        w(0x23), w(0xc3), w(0x18), w(0x96), w(0x05), w(0x9a), w(0x07),         \
        w(0x12), w(0x80), w(0xe2), w(0xeb), w(0x27), w(0xb2), w(0x75),         \
        w(0x09), w(0x83), w(0x2c), w(0x1a), w(0x1b), w(0x6e), w(0x5a),         \
        w(0xa0), w(0x52), w(0x3b), w(0xd6), w(0xb3), w(0x29), w(0xe3),         \
        w(0x2f), w(0x84), w(0x53), w(0xd1), w(0x00), w(0xed), w(0x20),         \
        w(0xfc), w(0xb1), w(0x5b), w(0x6a), w(0xcb), w(0xbe), w(0x39),         \
        w(0x4a), w(0x4c), w(0x58), w(0xcf), w(0xd0), w(0xef), w(0xaa),         \
        w(0xfb), w(0x43), w(0x4d), w(0x33), w(0x85), w(0x45), w(0xf9),         \
        w(0x02), w(0x7f), w(0x50), w(0x3c), w(0x9f), w(0xa8), w(0x51),         \
        w(0xa3), w(0x40), w(0x8f), w(0x92), w(0x9d), w(0x38), w(0xf5),         \
        w(0xbc), w(0xb6), w(0xda), w(0x21), w(0x10), w(0xff), w(0xf3),         \
        w(0xd2), w(0xcd), w(0x0c), w(0x13), w(0xec), w(0x5f), w(0x97),         \
        w(0x44), w(0x17), w(0xc4), w(0xa7), w(0x7e), w(0x3d), w(0x64),         \
        w(0x5d), w(0x19), w(0x73), w(0x60), w(0x81), w(0x4f), w(0xdc),         \
        w(0x22), w(0x2a), w(0x90), w(0x88), w(0x46), w(0xee), w(0xb8),         \
        w(0x14), w(0xde), w(0x5e), w(0x0b), w(0xdb), w(0xe0), w(0x32),         \
        w(0x3a), w(0x0a), w(0x49), w(0x06), w(0x24), w(0x5c), w(0xc2),         \
        w(0xd3), w(0xac), w(0x62), w(0x91), w(0x95), w(0xe4), w(0x79),         \
        w(0xe7), w(0xc8), w(0x37), w(0x6d), w(0x8d), w(0xd5), w(0x4e),         \
        w(0xa9), w(0x6c), w(0x56), w(0xf4), w(0xea), w(0x65), w(0x7a),         \
        w(0xae), w(0x08), w(0xba), w(0x78), w(0x25), w(0x2e), w(0x1c),         \
        w(0xa6), w(0xb4), w(0xc6), w(0xe8), w(0xdd), w(0x74), w(0x1f),         \
        w(0x4b), w(0xbd), w(0x8b), w(0x8a), w(0x70), w(0x3e), w(0xb5),         \
        w(0x66), w(0x48), w(0x03), w(0xf6), w(0x0e), w(0x61), w(0x35),         \
        w(0x57), w(0xb9), w(0x86), w(0xc1), w(0x1d), w(0x9e), w(0xe1),         \
        w(0xf8), w(0x98), w(0x11), w(0x69), w(0xd9), w(0x8e), w(0x94),         \
        w(0x9b), w(0x1e), w(0x87), w(0xe9), w(0xce), w(0x55), w(0x28),         \
        w(0xdf), w(0x8c), w(0xa1), w(0x89), w(0x0d), w(0xbf), w(0xe6),         \
        w(0x42), w(0x68), w(0x41), w(0x99), w(0x2d), w(0x0f), w(0xb0),         \
        w(0x54), w(0xbb), w(0x16)                                              \
  }

#define SAES_WPOLY 0x011b

#define saes_b2w(b0, b1, b2, b3)                                               \
  (((uint32_t)(b3) << 24) | ((uint32_t)(b2) << 16) | ((uint32_t)(b1) << 8) |   \
   (b0))

#define saes_f2(x) ((x << 1) ^ (((x >> 7) & 1) * SAES_WPOLY))
#define saes_f3(x) (saes_f2(x) ^ x)
#define saes_h0(x) (x)

#define saes_u0(p) saes_b2w(saes_f2(p), p, p, saes_f3(p))
#define saes_u1(p) saes_b2w(saes_f3(p), saes_f2(p), p, p)
#define saes_u2(p) saes_b2w(p, saes_f3(p), saes_f2(p), p)
#define saes_u3(p) saes_b2w(p, p, saes_f3(p), saes_f2(p))

static const uint32_t saes_table[4][256] = {
    saes_data(saes_u0), saes_data(saes_u1), saes_data(saes_u2),
    saes_data(saes_u3)};

static const unsigned char rc[40][16] = {
    {0x9d, 0x7b, 0x81, 0x75, 0xf0, 0xfe, 0xc5, 0xb2, 0x0a, 0xc0, 0x20, 0xe6,
     0x4c, 0x70, 0x84, 0x06},
    {0x17, 0xf7, 0x08, 0x2f, 0xa4, 0x6b, 0x0f, 0x64, 0x6b, 0xa0, 0xf3, 0x88,
     0xe1, 0xb4, 0x66, 0x8b},
    {0x14, 0x91, 0x02, 0x9f, 0x60, 0x9d, 0x02, 0xcf, 0x98, 0x84, 0xf2, 0x53,
     0x2d, 0xde, 0x02, 0x34},
    {0x79, 0x4f, 0x5b, 0xfd, 0xaf, 0xbc, 0xf3, 0xbb, 0x08, 0x4f, 0x7b, 0x2e,
     0xe6, 0xea, 0xd6, 0x0e},
    {0x44, 0x70, 0x39, 0xbe, 0x1c, 0xcd, 0xee, 0x79, 0x8b, 0x44, 0x72, 0x48,
     0xcb, 0xb0, 0xcf, 0xcb},
    {0x7b, 0x05, 0x8a, 0x2b, 0xed, 0x35, 0x53, 0x8d, 0xb7, 0x32, 0x90, 0x6e,
     0xee, 0xcd, 0xea, 0x7e},
    {0x1b, 0xef, 0x4f, 0xda, 0x61, 0x27, 0x41, 0xe2, 0xd0, 0x7c, 0x2e, 0x5e,
     0x43, 0x8f, 0xc2, 0x67},
    {0x3b, 0x0b, 0xc7, 0x1f, 0xe2, 0xfd, 0x5f, 0x67, 0x07, 0xcc, 0xca, 0xaf,
     0xb0, 0xd9, 0x24, 0x29},
    {0xee, 0x65, 0xd4, 0xb9, 0xca, 0x8f, 0xdb, 0xec, 0xe9, 0x7f, 0x86, 0xe6,
     0xf1, 0x63, 0x4d, 0xab},
    {0x33, 0x7e, 0x03, 0xad, 0x4f, 0x40, 0x2a, 0x5b, 0x64, 0xcd, 0xb7, 0xd4,
     0x84, 0xbf, 0x30, 0x1c},
    {0x00, 0x98, 0xf6, 0x8d, 0x2e, 0x8b, 0x02, 0x69, 0xbf, 0x23, 0x17, 0x94,
     0xb9, 0x0b, 0xcc, 0xb2},
    {0x8a, 0x2d, 0x9d, 0x5c, 0xc8, 0x9e, 0xaa, 0x4a, 0x72, 0x55, 0x6f, 0xde,
     0xa6, 0x78, 0x04, 0xfa},
    {0xd4, 0x9f, 0x12, 0x29, 0x2e, 0x4f, 0xfa, 0x0e, 0x12, 0x2a, 0x77, 0x6b,
     0x2b, 0x9f, 0xb4, 0xdf},
    {0xee, 0x12, 0x6a, 0xbb, 0xae, 0x11, 0xd6, 0x32, 0x36, 0xa2, 0x49, 0xf4,
     0x44, 0x03, 0xa1, 0x1e},
    {0xa6, 0xec, 0xa8, 0x9c, 0xc9, 0x00, 0x96, 0x5f, 0x84, 0x00, 0x05, 0x4b,
     0x88, 0x49, 0x04, 0xaf},
    {0xec, 0x93, 0xe5, 0x27, 0xe3, 0xc7, 0xa2, 0x78, 0x4f, 0x9c, 0x19, 0x9d,
     0xd8, 0x5e, 0x02, 0x21},
    {0x73, 0x01, 0xd4, 0x82, 0xcd, 0x2e, 0x28, 0xb9, 0xb7, 0xc9, 0x59, 0xa7,
     0xf8, 0xaa, 0x3a, 0xbf},
    {0x6b, 0x7d, 0x30, 0x10, 0xd9, 0xef, 0xf2, 0x37, 0x17, 0xb0, 0x86, 0x61,
     0x0d, 0x70, 0x60, 0x62},
    {0xc6, 0x9a, 0xfc, 0xf6, 0x53, 0x91, 0xc2, 0x81, 0x43, 0x04, 0x30, 0x21,
     0xc2, 0x45, 0xca, 0x5a},
    {0x3a, 0x94, 0xd1, 0x36, 0xe8, 0x92, 0xaf, 0x2c, 0xbb, 0x68, 0x6b, 0x22,
     0x3c, 0x97, 0x23, 0x92},
    {0xb4, 0x71, 0x10, 0xe5, 0x58, 0xb9, 0xba, 0x6c, 0xeb, 0x86, 0x58, 0x22,
     0x38, 0x92, 0xbf, 0xd3},
    {0x8d, 0x12, 0xe1, 0x24, 0xdd, 0xfd, 0x3d, 0x93, 0x77, 0xc6, 0xf0, 0xae,
     0xe5, 0x3c, 0x86, 0xdb},
    {0xb1, 0x12, 0x22, 0xcb, 0xe3, 0x8d, 0xe4, 0x83, 0x9c, 0xa0, 0xeb, 0xff,
     0x68, 0x62, 0x60, 0xbb},
    {0x7d, 0xf7, 0x2b, 0xc7, 0x4e, 0x1a, 0xb9, 0x2d, 0x9c, 0xd1, 0xe4, 0xe2,
     0xdc, 0xd3, 0x4b, 0x73},
    {0x4e, 0x92, 0xb3, 0x2c, 0xc4, 0x15, 0x14, 0x4b, 0x43, 0x1b, 0x30, 0x61,
     0xc3, 0x47, 0xbb, 0x43},
    {0x99, 0x68, 0xeb, 0x16, 0xdd, 0x31, 0xb2, 0x03, 0xf6, 0xef, 0x07, 0xe7,
     0xa8, 0x75, 0xa7, 0xdb},
    {0x2c, 0x47, 0xca, 0x7e, 0x02, 0x23, 0x5e, 0x8e, 0x77, 0x59, 0x75, 0x3c,
     0x4b, 0x61, 0xf3, 0x6d},
    {0xf9, 0x17, 0x86, 0xb8, 0xb9, 0xe5, 0x1b, 0x6d, 0x77, 0x7d, 0xde, 0xd6,
     0x17, 0x5a, 0xa7, 0xcd},
    {0x5d, 0xee, 0x46, 0xa9, 0x9d, 0x06, 0x6c, 0x9d, 0xaa, 0xe9, 0xa8, 0x6b,
     0xf0, 0x43, 0x6b, 0xec},
    {0xc1, 0x27, 0xf3, 0x3b, 0x59, 0x11, 0x53, 0xa2, 0x2b, 0x33, 0x57, 0xf9,
     0x50, 0x69, 0x1e, 0xcb},
    {0xd9, 0xd0, 0x0e, 0x60, 0x53, 0x03, 0xed, 0xe4, 0x9c, 0x61, 0xda, 0x00,
     0x75, 0x0c, 0xee, 0x2c},
    {0x50, 0xa3, 0xa4, 0x63, 0xbc, 0xba, 0xbb, 0x80, 0xab, 0x0c, 0xe9, 0x96,
     0xa1, 0xa5, 0xb1, 0xf0},
    {0x39, 0xca, 0x8d, 0x93, 0x30, 0xde, 0x0d, 0xab, 0x88, 0x29, 0x96, 0x5e,
     0x02, 0xb1, 0x3d, 0xae},
    {0x42, 0xb4, 0x75, 0x2e, 0xa8, 0xf3, 0x14, 0x88, 0x0b, 0xa4, 0x54, 0xd5,
     0x38, 0x8f, 0xbb, 0x17},
    {0xf6, 0x16, 0x0a, 0x36, 0x79, 0xb7, 0xb6, 0xae, 0xd7, 0x7f, 0x42, 0x5f,
     0x5b, 0x8a, 0xbb, 0x34},
    {0xde, 0xaf, 0xba, 0xff, 0x18, 0x59, 0xce, 0x43, 0x38, 0x54, 0xe5, 0xcb,
     0x41, 0x52, 0xf6, 0x26},
    {0x78, 0xc9, 0x9e, 0x83, 0xf7, 0x9c, 0xca, 0xa2, 0x6a, 0x02, 0xf3, 0xb9,
     0x54, 0x9a, 0xe9, 0x4c},
    {0x35, 0x12, 0x90, 0x22, 0x28, 0x6e, 0xc0, 0x40, 0xbe, 0xf7, 0xdf, 0x1b,
     0x1a, 0xa5, 0x51, 0xae},
    {0xcf, 0x59, 0xa6, 0x48, 0x0f, 0xbc, 0x73, 0xc1, 0x2b, 0xd2, 0x7e, 0xba,
     0x3c, 0x61, 0xc1, 0xa0},
    {0xa1, 0x9d, 0xc5, 0xe9, 0xfd, 0xbd, 0xd6, 0x4a, 0x88, 0x82, 0x28, 0x02,
     0x03, 0xcc, 0x6a, 0x75}};

// static const unsigned char sbox[256] = {
//     0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b,
//     0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
//     0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26,
//     0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
//     0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
//     0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
//     0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed,
//     0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
//     0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f,
//     0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
//     0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec,
//     0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
//     0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14,
//     0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
//     0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
//     0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
//     0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f,
//     0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
//     0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
//     0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
//     0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f,
//     0xb0, 0x54, 0xbb, 0x16};

#define XT(x) (((x) << 1) ^ ((((x) >> 7) & 1) * 0x1b))

// Simulate _mm_aesenc_si128 instructions from AESNI

void aesenc(unsigned char *s, const unsigned char *rk) {
  // #define XT(x) (((x) << 1) ^ (((x) >> 7) ? 0x1b : 0))
  const uint32_t *t = saes_table[0];
  // #define XT4(x) ((((x) << 1) & 0xfefefefe) ^ ((((x) >> 31) & 1) ? 0x1b000000
  // : 0)^ ((((x) >> 23)&1) ? 0x001b0000 : 0)^ ((((x) >> 15)&1) ? 0x00001b00 :
  // 0)^ ((((x) >> 7)&1) ? 0x0000001b : 0))
  uint32_t x0 = ((uint32_t *)s)[0];
  uint32_t x1 = ((uint32_t *)s)[1];
  uint32_t x2 = ((uint32_t *)s)[2];
  uint32_t x3 = ((uint32_t *)s)[3];

  uint32_t y0 = t[x0 & 0xff];
  x0 >>= 8;
  uint32_t y1 = t[x1 & 0xff];
  x1 >>= 8;
  uint32_t y2 = t[x2 & 0xff];
  x2 >>= 8;
  uint32_t y3 = t[x3 & 0xff];
  x3 >>= 8;
  t += 256;

  y0 ^= t[x1 & 0xff];
  x1 >>= 8;
  y1 ^= t[x2 & 0xff];
  x2 >>= 8;
  y2 ^= t[x3 & 0xff];
  x3 >>= 8;
  y3 ^= t[x0 & 0xff];
  x0 >>= 8;
  t += 256;

  y0 ^= t[x2 & 0xff];
  x2 >>= 8;
  y1 ^= t[x3 & 0xff];
  x3 >>= 8;
  y2 ^= t[x0 & 0xff];
  x0 >>= 8;
  y3 ^= t[x1 & 0xff];
  x1 >>= 8;
  t += 256;

  y0 ^= t[x3];
  y1 ^= t[x0];
  y2 ^= t[x1];
  y3 ^= t[x2];

  ((uint32_t *)s)[0] = y0 ^ ((uint32_t *)rk)[0];
  ((uint32_t *)s)[1] = y1 ^ ((uint32_t *)rk)[1];
  ((uint32_t *)s)[2] = y2 ^ ((uint32_t *)rk)[2];
  ((uint32_t *)s)[3] = y3 ^ ((uint32_t *)rk)[3];
}

// static void aesenc2(unsigned char *s, const unsigned char *rk) {
//   unsigned char i, t, u, v[4][4];
//   for (i = 0; i < 16; ++i) {
//     v[((i / 4) + 4 - (i % 4)) % 4][i % 4] = sbox[s[i]];
//   }
//   for (i = 0; i < 4; ++i) {
//     t = v[i][0];
//     u = v[i][0] ^ v[i][1] ^ v[i][2] ^ v[i][3];
//     v[i][0] ^= u ^ XT(v[i][0] ^ v[i][1]);
//     v[i][1] ^= u ^ XT(v[i][1] ^ v[i][2]);
//     v[i][2] ^= u ^ XT(v[i][2] ^ v[i][3]);
//     v[i][3] ^= u ^ XT(v[i][3] ^ t);
//   }
//   for (i = 0; i < 16; ++i) {
//     s[i] = v[i / 4][i % 4] ^ rk[i];
//   }
// }

// Simulate _mm_unpacklo_epi32
void unpacklo32(unsigned char *t, unsigned char *a, unsigned char *b) {
  unsigned char tmp[16];
  memcpy(tmp, a, 4);
  memcpy(tmp + 4, b, 4);
  memcpy(tmp + 8, a + 4, 4);
  memcpy(tmp + 12, b + 4, 4);
  memcpy(t, tmp, 16);
}

// Simulate _mm_unpackhi_epi32
void unpackhi32(unsigned char *t, unsigned char *a, unsigned char *b) {
  unsigned char tmp[16];
  memcpy(tmp, a + 8, 4);
  memcpy(tmp + 4, b + 8, 4);
  memcpy(tmp + 8, a + 12, 4);
  memcpy(tmp + 12, b + 12, 4);
  memcpy(t, tmp, 16);
}

/* void load_constants_port(void) */
/* { */
/*     #<{(| Use the standard constants to generate tweaked ones. |)}># */
/*     memcpy(rc, haraka_rc, 40*16); */
/* } */

/* void tweak_constants(const unsigned char *pk_seed, const unsigned char
 * *sk_seed, */
/*                      unsigned long long seed_length) */
/* { */
/*     unsigned char buf[40*16]; */
/*  */
/*     #<{(| Use the standard constants to generate tweaked ones. |)}># */
/*     memcpy(rc, haraka_rc, 40*16); */
/*  */
/*     #<{(| Constants for sk.seed |)}># */
/*     if (sk_seed != NULL) { */
/*         haraka_S(buf, 40*16, sk_seed, seed_length); */
/*         memcpy(rc_sseed, buf, 40*16); */
/*     } */
/*  */
/*     #<{(| Constants for pk.seed |)}># */
/*     haraka_S(buf, 40*16, pk_seed, seed_length); */
/*     memcpy(rc, buf, 40*16);     */
/* } */

void haraka512_perm(unsigned char *out, const unsigned char *in) {
  int i, j;

  unsigned char s[64], tmp[16];

  memcpy(s, in, 16);
  memcpy(s + 16, in + 16, 16);
  memcpy(s + 32, in + 32, 16);
  memcpy(s + 48, in + 48, 16);

  for (i = 0; i < 5; ++i) {
    // aes round(s)
    for (j = 0; j < 2; ++j) {
      aesenc(s, rc[4 * 2 * i + 4 * j]);
      aesenc(s + 16, rc[4 * 2 * i + 4 * j + 1]);
      aesenc(s + 32, rc[4 * 2 * i + 4 * j + 2]);
      aesenc(s + 48, rc[4 * 2 * i + 4 * j + 3]);
    }

    // mixing
    unpacklo32(tmp, s, s + 16);
    unpackhi32(s, s, s + 16);
    unpacklo32(s + 16, s + 32, s + 48);
    unpackhi32(s + 32, s + 32, s + 48);
    unpacklo32(s + 48, s, s + 32);
    unpackhi32(s, s, s + 32);
    unpackhi32(s + 32, s + 16, tmp);
    unpacklo32(s + 16, s + 16, tmp);
  }

  memcpy(out, s, 64);
}

void haraka512_perm_keyed(unsigned char *out, const unsigned char *in,
                          const u128 *rc) {
  int i, j;

  unsigned char s[64], tmp[16];

  memcpy(s, in, 16);
  memcpy(s + 16, in + 16, 16);
  memcpy(s + 32, in + 32, 16);
  memcpy(s + 48, in + 48, 16);

  for (i = 0; i < 5; ++i) {
    // aes round(s)
    for (j = 0; j < 2; ++j) {
      aesenc(s, (const unsigned char *)&rc[4 * 2 * i + 4 * j]);
      aesenc(s + 16, (const unsigned char *)&rc[4 * 2 * i + 4 * j + 1]);
      aesenc(s + 32, (const unsigned char *)&rc[4 * 2 * i + 4 * j + 2]);
      aesenc(s + 48, (const unsigned char *)&rc[4 * 2 * i + 4 * j + 3]);
    }

    // mixing
    unpacklo32(tmp, s, s + 16);
    unpackhi32(s, s, s + 16);
    unpacklo32(s + 16, s + 32, s + 48);
    unpackhi32(s + 32, s + 32, s + 48);
    unpacklo32(s + 48, s, s + 32);
    unpackhi32(s, s, s + 32);
    unpackhi32(s + 32, s + 16, tmp);
    unpacklo32(s + 16, s + 16, tmp);
  }

  memcpy(out, s, 64);
}
}

void haraka512_port(unsigned char *out, const unsigned char *in) {
  int i;

  unsigned char buf[64];

  haraka512_perm(buf, in);
  /* Feed-forward */
  for (i = 0; i < 64; i++) {
    buf[i] = buf[i] ^ in[i];
  }

  /* Truncated */
  memcpy(out, buf + 8, 8);
  memcpy(out + 8, buf + 24, 8);
  memcpy(out + 16, buf + 32, 8);
  memcpy(out + 24, buf + 48, 8);
}

void haraka512_port_keyed(unsigned char *out, const unsigned char *in,
                          const u128 *rc) {
  int i;

  unsigned char buf[64];

  haraka512_perm_keyed(buf, in, rc);
  /* Feed-forward */
  for (i = 0; i < 64; i++) {
    buf[i] = buf[i] ^ in[i];
  }

  /* Truncated */
  memcpy(out, buf + 8, 8);
  memcpy(out + 8, buf + 24, 8);
  memcpy(out + 16, buf + 32, 8);
  memcpy(out + 24, buf + 48, 8);
}

static inline __attribute__((always_inline)) void
haraka512_port_keyed_local(unsigned char *out, const unsigned char *in,
                          const u128 *rc) {
  int i;

  unsigned char buf[64];

  haraka512_perm_keyed(buf, in, rc);
  /* Feed-forward */
  for (i = 0; i < 64; i++) {
    buf[i] = buf[i] ^ in[i];
  }

  /* Truncated */
  memcpy(out, buf + 8, 8);
  memcpy(out + 8, buf + 24, 8);
  memcpy(out + 16, buf + 32, 8);
  memcpy(out + 24, buf + 48, 8);
}

void haraka256_port(unsigned char *out, const unsigned char *in) {
  int i, j;

  unsigned char s[32], tmp[16];

  memcpy(s, in, 16);
  memcpy(s + 16, in + 16, 16);

  for (i = 0; i < 5; ++i) {
    // aes round(s)
    for (j = 0; j < 2; ++j) {
      aesenc(s, rc[2 * 2 * i + 2 * j]);
      aesenc(s + 16, rc[2 * 2 * i + 2 * j + 1]);
    }

    // mixing
    unpacklo32(tmp, s, s + 16);
    unpackhi32(s + 16, s, s + 16);
    memcpy(s, tmp, 16);
  }

  /* Feed-forward */
  for (i = 0; i < 32; i++) {
    out[i] = in[i] ^ s[i];
  }
}

namespace{

void clmul64(uint64_t a, uint64_t b, uint64_t *r) {
  uint8_t s = 4, i;           // window size
  uint64_t two_s = 1 << s;    // 2^s
  uint64_t smask = two_s - 1; // s 1 bits
  uint64_t u[16];
  uint64_t tmp;
  uint64_t ifmask;
  // Precomputation
  u[0] = 0;
  u[1] = b;
  for (i = 2; i < two_s; i += 2) {
    u[i] = u[i >> 1] << 1; // even indices: left shift
    u[i + 1] = u[i] ^ b;   // odd indices: xor b
  }
  // Multiply
  r[0] = u[a & smask]; // first window only affects lower word
  r[1] = 0;
  for (i = s; i < 64; i += s) {
    tmp = u[a >> i & smask];
    r[0] ^= tmp << i;
    r[1] ^= tmp >> (64 - i);
  }
  // Repair
  uint64_t m = 0xEEEEEEEEEEEEEEEE; // s=4 => 16 times 1110
  for (i = 1; i < s; i++) {
    tmp = ((a & m) >> i);
    m &= m << 1; // shift mask to exclude all bit j': j' mod s = i
    ifmask = -((b >> (64 - i)) & 1); // if the (64-i)th bit of b is 1
    r[1] ^= (tmp & ifmask);
  }
}

u128 _mm_clmulepi64_si128_emu(const __m128i &a, const __m128i &b, int imm) {
  uint64_t result[2];
  clmul64(*((uint64_t *)&a + (imm & 1)),
          *((uint64_t *)&b + ((imm & 0x10) >> 4)), result);

  /*
  // TEST
  const __m128i tmp1 = _mm_load_si128(&a);
  const __m128i tmp2 = _mm_load_si128(&b);
  imm = imm & 0x11;
  const __m128i testresult = (imm == 0x10) ? _mm_clmulepi64_si128(tmp1, tmp2,
  0x10) : ((imm == 0x01) ? _mm_clmulepi64_si128(tmp1, tmp2, 0x01) : ((imm ==
  0x00) ? _mm_clmulepi64_si128(tmp1, tmp2, 0x00) : _mm_clmulepi64_si128(tmp1,
  tmp2, 0x11))); if (!memcmp(&testresult, &result, 16))
  {
      printf("_mm_clmulepi64_si128_emu: Portable version passed!\n");
  }
  else
  {
      printf("_mm_clmulepi64_si128_emu: Portable version failed! a: %lxh %lxl,
  b: %lxh %lxl, imm: %x, emu: %lxh %lxl, intrin: %lxh %lxl\n",
             *((uint64_t *)&a + 1), *(uint64_t *)&a,
             *((uint64_t *)&b + 1), *(uint64_t *)&b,
             imm,
             *((uint64_t *)result + 1), *(uint64_t *)result,
             *((uint64_t *)&testresult + 1), *(uint64_t *)&testresult);
      return testresult;
  }
  */

  return *(__m128i *)result;
}

u128 _mm_mulhrs_epi16_emu(__m128i _a, __m128i _b) {
  int16_t result[8];
  int16_t *a = (int16_t *)&_a, *b = (int16_t *)&_b;
  for (int i = 0; i < 8; i++) {
    result[i] = (int16_t)((((int32_t)(a[i]) * (int32_t)(b[i])) + 0x4000) >> 15);
  }

  /*
  const __m128i testresult = _mm_mulhrs_epi16(_a, _b);
  if (!memcmp(&testresult, &result, 16))
  {
      printf("_mm_mulhrs_epi16_emu: Portable version passed!\n");
  }
  else
  {
      printf("_mm_mulhrs_epi16_emu: Portable version failed! a: %lxh %lxl, b:
  %lxh %lxl, emu: %lxh %lxl, intrin: %lxh %lxl\n",
             *((uint64_t *)&a + 1), *(uint64_t *)&a,
             *((uint64_t *)&b + 1), *(uint64_t *)&b,
             *((uint64_t *)result + 1), *(uint64_t *)result,
             *((uint64_t *)&testresult + 1), *(uint64_t *)&testresult);
  }
  */

  return *(__m128i *)result;
}

inline u128 _mm_set_epi64x_emu(uint64_t hi, uint64_t lo) {
  __m128i result;
  ((uint64_t *)&result)[0] = lo;
  ((uint64_t *)&result)[1] = hi;
  return result;
}

inline u128 _mm_cvtsi64_si128_emu(uint64_t lo) {
  __m128i result;
  ((uint64_t *)&result)[0] = lo;
  ((uint64_t *)&result)[1] = 0;
  return result;
}

inline int64_t _mm_cvtsi128_si64_emu(const __m128i &a) {
  return *(const int64_t *)&a;
}

// inline int32_t _mm_cvtsi128_si32_emu(const __m128i &a) {
//   return *(const int32_t *)&a;
// }

inline u128 _mm_cvtsi32_si128_emu(uint32_t lo) {
  __m128i result;
  ((uint32_t *)&result)[0] = lo;
  ((uint32_t *)&result)[1] = 0;
  ((uint64_t *)&result)[1] = 0;

  /*
  const __m128i testresult = _mm_cvtsi32_si128(lo);
  if (!memcmp(&testresult, &result, 16))
  {
      printf("_mm_cvtsi32_si128_emu: Portable version passed!\n");
  }
  else
  {
      printf("_mm_cvtsi32_si128_emu: Portable version failed!\n");
  }
  */

  return result;
}

u128 _mm_setr_epi8_emu(uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4,
                       uint8_t c5, uint8_t c6, uint8_t c7, uint8_t c8, uint8_t c9,
                       uint8_t c10, uint8_t c11, uint8_t c12, uint8_t c13,
                       uint8_t c14, uint8_t c15) {
  __m128i result;
  ((uint8_t *)&result)[0] = c0;
  ((uint8_t *)&result)[1] = c1;
  ((uint8_t *)&result)[2] = c2;
  ((uint8_t *)&result)[3] = c3;
  ((uint8_t *)&result)[4] = c4;
  ((uint8_t *)&result)[5] = c5;
  ((uint8_t *)&result)[6] = c6;
  ((uint8_t *)&result)[7] = c7;
  ((uint8_t *)&result)[8] = c8;
  ((uint8_t *)&result)[9] = c9;
  ((uint8_t *)&result)[10] = c10;
  ((uint8_t *)&result)[11] = c11;
  ((uint8_t *)&result)[12] = c12;
  ((uint8_t *)&result)[13] = c13;
  ((uint8_t *)&result)[14] = c14;
  ((uint8_t *)&result)[15] = c15;

  /*
  const __m128i testresult =
  _mm_setr_epi8(c0,c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15); if
  (!memcmp(&testresult, &result, 16))
  {
      printf("_mm_setr_epi8_emu: Portable version passed!\n");
  }
  else
  {
      printf("_mm_setr_epi8_emu: Portable version failed!\n");
  }
  */

  return result;
}

inline __m128i _mm_srli_si128_emu(__m128i a, int imm8) {
  unsigned char result[16];
  uint8_t shift = imm8 & 0xff;
  if (shift > 15)
    shift = 16;

  int i;
  for (i = 0; i < (16 - shift); i++) {
    result[i] = ((unsigned char *)&a)[shift + i];
  }
  for (; i < 16; i++) {
    result[i] = 0;
  }

  /*
  const __m128i tmp1 = _mm_load_si128(&a);
  __m128i testresult = _mm_srli_si128(tmp1, imm8);
  if (!memcmp(&testresult, result, 16))
  {
      printf("_mm_srli_si128_emu: Portable version passed!\n");
  }
  else
  {
      printf("_mm_srli_si128_emu: Portable version failed! val: %lx%lx imm: %x
  emu: %lx%lx, intrin: %lx%lx\n",
             *((uint64_t *)&a + 1), *(uint64_t *)&a,
             imm8,
             *((uint64_t *)result + 1), *(uint64_t *)result,
             *((uint64_t *)&testresult + 1), *(uint64_t *)&testresult);
  }
  */

  return *(__m128i *)result;
}

inline __m128i _mm_xor_si128_emu(__m128i a, __m128i b) {
#ifdef _WIN32
  uint64_t result[2];
  result[0] = *(uint64_t *)&a ^ *(uint64_t *)&b;
  result[1] = *((uint64_t *)&a + 1) ^ *((uint64_t *)&b + 1);
  return *(__m128i *)result;
#else
  return a ^ b;
#endif
}

inline __m128i _mm_load_si128_emu(const void *p) { return *(__m128i *)p; }

inline void _mm_store_si128_emu(void *p, __m128i val) { *(__m128i *)p = val; }

__m128i _mm_shuffle_epi8_emu(__m128i a, __m128i b) {
  __m128i result;
  for (int i = 0; i < 16; i++) {
    if (((uint8_t *)&b)[i] & 0x80) {
      ((uint8_t *)&result)[i] = 0;
    } else {
      ((uint8_t *)&result)[i] = ((uint8_t *)&a)[((uint8_t *)&b)[i] & 0xf];
    }
  }

  /*
  const __m128i tmp1 = _mm_load_si128(&a);
  const __m128i tmp2 = _mm_load_si128(&b);
  __m128i testresult = _mm_shuffle_epi8(tmp1, tmp2);
  if (!memcmp(&testresult, &result, 16))
  {
      printf("_mm_shuffle_epi8_emu: Portable version passed!\n");
  }
  else
  {
      printf("_mm_shuffle_epi8_emu: Portable version failed!\n");
  }
  */

  return result;
}

}
// portable
static inline __m128i lazyLengthHash_port(uint64_t keylength, uint64_t length) {
  const __m128i lengthvector = _mm_set_epi64x_emu(keylength, length);
  const __m128i clprod1 =
      _mm_clmulepi64_si128_emu(lengthvector, lengthvector, 0x10);
  return clprod1;
}

// modulo reduction to 64-bit value. The high 64 bits contain garbage, see
// precompReduction64
static inline __m128i precompReduction64_si128_port(__m128i A) {

  // const __m128i C = _mm_set_epi64x(1U,(1U<<4)+(1U<<3)+(1U<<1)+(1U<<0)); // C
  // is the irreducible poly. (64,4,3,1,0)
  const __m128i C =
      _mm_cvtsi64_si128_emu((1U << 4) + (1U << 3) + (1U << 1) + (1U << 0));
  __m128i Q2 = _mm_clmulepi64_si128_emu(A, C, 0x01);
  __m128i Q3 = _mm_shuffle_epi8_emu(
      _mm_setr_epi8_emu(0, 27, 54, 45, 108, 119, 90, 65, (char)216, (char)195,
                        (char)238, (char)245, (char)180, (char)175, (char)130,
                        (char)153),
      _mm_srli_si128_emu(Q2, 8));
  __m128i Q4 = _mm_xor_si128_emu(Q2, A);
  const __m128i final = _mm_xor_si128_emu(Q3, Q4);
  return final; /// WARNING: HIGH 64 BITS SHOULD BE ASSUMED TO CONTAIN GARBAGE
}

static inline uint64_t precompReduction64_port(__m128i A) {
  __m128i tmp = precompReduction64_si128_port(A);
  return _mm_cvtsi128_si64_emu(tmp);
}

// verus intermediate hash extra
__m128i __verusclmulwithoutreduction64alignedrepeat_port(
    __m128i *randomsource, const __m128i buf[4], uint64_t keyMask,
    __m128i **pMoveScratch) {
  __m128i const *pbuf;

  /*
  std::cout << "Random key start: ";
  std::cout << LEToHex(*randomsource) << ", ";
  std::cout << LEToHex(*(randomsource + 1));
  std::cout << std::endl;
  */

  // divide key mask by 16 from bytes to __m128i
  keyMask >>= 4;

  // the random buffer must have at least 32 16 byte dwords after the keymask to
  // work with this algorithm. we take the value from the last element inside
  // the keyMask + 2, as that will never be used to xor into the accumulator
  // before it is hashed with other values first
  __m128i acc = _mm_load_si128_emu(randomsource + (keyMask + 2));

  for (int64_t i = 0; i < 32; i++) {
    // std::cout << "LOOP " << i << " acc: " << LEToHex(acc) << std::endl;

    const uint64_t selector = _mm_cvtsi128_si64_emu(acc);

    // get two random locations in the key, which will be mutated and swapped
    __m128i *prand = randomsource + ((selector >> 5) & keyMask);
    __m128i *prandex = randomsource + ((selector >> 32) & keyMask);

    *pMoveScratch++ = prand;
    *pMoveScratch++ = prandex;

    // select random start and order of pbuf processing
    pbuf = buf + (selector & 3);

    switch (selector & 0x1c) {
    case 0: {
      const __m128i temp1 = _mm_load_si128_emu(prandex);
      const __m128i temp2 =
          _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
      const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
      acc = _mm_xor_si128_emu(clprod1, acc);

      /*
      std::cout << "temp1: " << LEToHex(temp1) << std::endl;
      std::cout << "temp2: " << LEToHex(temp2) << std::endl;
      std::cout << "add1: " << LEToHex(add1) << std::endl;
      std::cout << "clprod1: " << LEToHex(clprod1) << std::endl;
      std::cout << "acc: " << LEToHex(acc) << std::endl;
      */

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

      const __m128i temp12 = _mm_load_si128_emu(prand);
      _mm_store_si128_emu(prand, tempa2);

      const __m128i temp22 = _mm_load_si128_emu(pbuf);
      const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
      const __m128i clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
      acc = _mm_xor_si128_emu(clprod12, acc);

      const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
      const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
      _mm_store_si128_emu(prandex, tempb2);
      break;
    }
    case 4: {
      const __m128i temp1 = _mm_load_si128_emu(prand);
      const __m128i temp2 = _mm_load_si128_emu(pbuf);
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
      const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
      acc = _mm_xor_si128_emu(clprod1, acc);
      const __m128i clprod2 = _mm_clmulepi64_si128_emu(temp2, temp2, 0x10);
      acc = _mm_xor_si128_emu(clprod2, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

      const __m128i temp12 = _mm_load_si128_emu(prandex);
      _mm_store_si128_emu(prandex, tempa2);

      const __m128i temp22 =
          _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
      acc = _mm_xor_si128_emu(add12, acc);

      const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
      const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
      _mm_store_si128_emu(prand, tempb2);
      break;
    }
    case 8: {
      const __m128i temp1 = _mm_load_si128_emu(prandex);
      const __m128i temp2 = _mm_load_si128_emu(pbuf);
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
      acc = _mm_xor_si128_emu(add1, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

      const __m128i temp12 = _mm_load_si128_emu(prand);
      _mm_store_si128_emu(prand, tempa2);

      const __m128i temp22 =
          _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
      const __m128i clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
      acc = _mm_xor_si128_emu(clprod12, acc);
      const __m128i clprod22 = _mm_clmulepi64_si128_emu(temp22, temp22, 0x10);
      acc = _mm_xor_si128_emu(clprod22, acc);

      const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
      const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
      _mm_store_si128_emu(prandex, tempb2);
      break;
    }
    case 0xc: {
      const __m128i temp1 = _mm_load_si128_emu(prand);
      const __m128i temp2 =
          _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);

      // cannot be zero here
      const int32_t divisor = (uint32_t)selector;

      acc = _mm_xor_si128_emu(add1, acc);

      const int64_t dividend = _mm_cvtsi128_si64_emu(acc);
      const __m128i modulo = _mm_cvtsi32_si128_emu(dividend % divisor);
      acc = _mm_xor_si128_emu(modulo, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

      if (dividend & 1) {
        const __m128i temp12 = _mm_load_si128_emu(prandex);
        _mm_store_si128_emu(prandex, tempa2);

        const __m128i temp22 = _mm_load_si128_emu(pbuf);
        const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
        const __m128i clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
        acc = _mm_xor_si128_emu(clprod12, acc);
        const __m128i clprod22 = _mm_clmulepi64_si128_emu(temp22, temp22, 0x10);
        acc = _mm_xor_si128_emu(clprod22, acc);

        const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
        const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
        _mm_store_si128_emu(prand, tempb2);
      } else {
        const __m128i tempb3 = _mm_load_si128_emu(prandex);
        _mm_store_si128_emu(prandex, tempa2);
        _mm_store_si128_emu(prand, tempb3);
      }
      break;
    }
    case 0x10: {
      // a few AES operations
      const __m128i *rc = prand;
      __m128i tmp;

      __m128i temp1 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      __m128i temp2 = _mm_load_si128_emu(pbuf);

      AES2_EMU(temp1, temp2, 0);
      MIX2_EMU(temp1, temp2);

      AES2_EMU(temp1, temp2, 4);
      MIX2_EMU(temp1, temp2);

      AES2_EMU(temp1, temp2, 8);
      MIX2_EMU(temp1, temp2);

      acc = _mm_xor_si128_emu(temp1, acc);
      acc = _mm_xor_si128_emu(temp2, acc);

      const __m128i tempa1 = _mm_load_si128_emu(prand);
      const __m128i tempa2 = _mm_mulhrs_epi16_emu(acc, tempa1);
      const __m128i tempa3 = _mm_xor_si128_emu(tempa1, tempa2);

      const __m128i tempa4 = _mm_load_si128_emu(prandex);
      _mm_store_si128_emu(prandex, tempa3);
      _mm_store_si128_emu(prand, tempa4);
      break;
    }
    case 0x14: {
      // we'll just call this one the monkins loop, inspired by Chris
      const __m128i *buftmp = pbuf - (((selector & 1) << 1) - 1);
      __m128i tmp; // used by MIX2

      uint64_t rounds = selector >> 61; // loop randomly between 1 and 8 times
      __m128i *rc = prand;
      uint64_t aesround = 0;
      __m128i onekey;

      do {
        // std::cout << "acc: " << LEToHex(acc) << ", round check: " <<
        // LEToHex((selector & (0x10000000 << rounds))) << std::endl;

        // note that due to compiler and CPUs, we expect this to do:
        // if (selector & ((0x10000000 << rounds) & 0xffffffff) if rounds != 3
        // else selector & 0xffffffff80000000):
        if (selector & (0x10000000 << rounds)) {
          onekey = _mm_load_si128_emu(rc++);
          const __m128i temp2 = _mm_load_si128_emu(rounds & 1 ? pbuf : buftmp);
          const __m128i add1 = _mm_xor_si128_emu(onekey, temp2);
          const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
          acc = _mm_xor_si128_emu(clprod1, acc);
        } else {
          onekey = _mm_load_si128_emu(rc++);
          __m128i temp2 = _mm_load_si128_emu(rounds & 1 ? buftmp : pbuf);
          const uint64_t roundidx = aesround++ << 2;
          AES2_EMU(onekey, temp2, roundidx);

          /*
          std::cout << " onekey1: " << LEToHex(onekey) << std::endl;
          std::cout << "  temp21: " << LEToHex(temp2) << std::endl;
          std::cout << "roundkey: " << LEToHex(rc[roundidx]) << std::endl;

          aesenc((unsigned char *)&onekey, (unsigned char *)&(rc[roundidx]));

          std::cout << "onekey2: " << LEToHex(onekey) << std::endl;
          std::cout << "roundkey: " << LEToHex(rc[roundidx + 1]) << std::endl;

          aesenc((unsigned char *)&temp2, (unsigned char *)&(rc[roundidx + 1]));

          std::cout << " temp22: " << LEToHex(temp2) << std::endl;
          std::cout << "roundkey: " << LEToHex(rc[roundidx + 2]) << std::endl;

          aesenc((unsigned char *)&onekey, (unsigned char *)&(rc[roundidx +
          2]));

          std::cout << "onekey2: " << LEToHex(onekey) << std::endl;

          aesenc((unsigned char *)&temp2, (unsigned char *)&(rc[roundidx + 3]));

          std::cout << " temp22: " << LEToHex(temp2) << std::endl;
          */

          MIX2_EMU(onekey, temp2);

          /*
          std::cout << "onekey3: " << LEToHex(onekey) << std::endl;
          */

          acc = _mm_xor_si128_emu(onekey, acc);
          acc = _mm_xor_si128_emu(temp2, acc);
        }
      } while (rounds--);

      const __m128i tempa1 = _mm_load_si128_emu(prand);
      const __m128i tempa2 = _mm_mulhrs_epi16_emu(acc, tempa1);
      const __m128i tempa3 = _mm_xor_si128_emu(tempa1, tempa2);

      const __m128i tempa4 = _mm_load_si128_emu(prandex);
      _mm_store_si128_emu(prandex, tempa3);
      _mm_store_si128_emu(prand, tempa4);
      break;
    }
    case 0x18: {
      const __m128i temp1 =
          _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      const __m128i temp2 = _mm_load_si128_emu(prand);
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
      const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
      acc = _mm_xor_si128_emu(clprod1, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp2);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp2);

      const __m128i tempb3 = _mm_load_si128_emu(prandex);
      _mm_store_si128_emu(prandex, tempa2);
      _mm_store_si128_emu(prand, tempb3);
      break;
    }
    case 0x1c: {
      const __m128i temp1 = _mm_load_si128_emu(pbuf);
      const __m128i temp2 = _mm_load_si128_emu(prandex);
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
      const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
      acc = _mm_xor_si128_emu(clprod1, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp2);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp2);

      const __m128i tempa3 = _mm_load_si128_emu(prand);
      _mm_store_si128_emu(prand, tempa2);

      acc = _mm_xor_si128_emu(tempa3, acc);

      const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, tempa3);
      const __m128i tempb2 = _mm_xor_si128_emu(tempb1, tempa3);
      _mm_store_si128_emu(prandex, tempb2);
      break;
    }
    }
  }
  return acc;
}

// verus intermediate hash extra
__m128i __verusclmulwithoutreduction64alignedrepeat_sv2_1_port(
    __m128i *randomsource, const __m128i buf[4], uint64_t keyMask,
    __m128i **pMoveScratch) {
  const __m128i pbuf_copy[4] = {_mm_xor_si128(buf[0], buf[2]),
                                _mm_xor_si128(buf[1], buf[3]), buf[2], buf[3]};
  const __m128i *pbuf;

  // divide key mask by 16 from bytes to __m128i
  keyMask >>= 4;

  // the random buffer must have at least 32 16 byte dwords after the keymask to
  // work with this algorithm. we take the value from the last element inside
  // the keyMask + 2, as that will never be used to xor into the accumulator
  // before it is hashed with other values first
  __m128i acc = _mm_load_si128_emu(randomsource + (keyMask + 2));

  for (int64_t i = 0; i < 32; i++) {
    // std::cout << "LOOP " << i << " acc: " << LEToHex(acc) << std::endl;

    const uint64_t selector = _mm_cvtsi128_si64_emu(acc);

    // get two random locations in the key, which will be mutated and swapped
    __m128i *prand = randomsource + ((selector >> 5) & keyMask);
    __m128i *prandex = randomsource + ((selector >> 32) & keyMask);

    *pMoveScratch++ = prand;
    *pMoveScratch++ = prandex;

    // select random start and order of pbuf processing
    pbuf = pbuf_copy + (selector & 3);

    switch (selector & 0x1c) {
    case 0: {
      const __m128i temp1 = _mm_load_si128_emu(prandex);
      const __m128i temp2 =
          _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
      const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
      acc = _mm_xor_si128_emu(clprod1, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

      const __m128i temp12 = _mm_load_si128_emu(prand);
      _mm_store_si128_emu(prand, tempa2);

      const __m128i temp22 = _mm_load_si128_emu(pbuf);
      const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
      const __m128i clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
      acc = _mm_xor_si128_emu(clprod12, acc);

      const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
      const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
      _mm_store_si128_emu(prandex, tempb2);
      break;
    }
    case 4: {
      const __m128i temp1 = _mm_load_si128_emu(prand);
      const __m128i temp2 = _mm_load_si128_emu(pbuf);
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
      const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
      acc = _mm_xor_si128_emu(clprod1, acc);
      const __m128i clprod2 = _mm_clmulepi64_si128_emu(temp2, temp2, 0x10);
      acc = _mm_xor_si128_emu(clprod2, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

      const __m128i temp12 = _mm_load_si128_emu(prandex);
      _mm_store_si128_emu(prandex, tempa2);

      const __m128i temp22 =
          _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
      acc = _mm_xor_si128_emu(add12, acc);

      const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
      const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
      _mm_store_si128_emu(prand, tempb2);
      break;
    }
    case 8: {
      const __m128i temp1 = _mm_load_si128_emu(prandex);
      const __m128i temp2 = _mm_load_si128_emu(pbuf);
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
      acc = _mm_xor_si128_emu(add1, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

      const __m128i temp12 = _mm_load_si128_emu(prand);
      _mm_store_si128_emu(prand, tempa2);

      const __m128i temp22 =
          _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
      const __m128i clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
      acc = _mm_xor_si128_emu(clprod12, acc);
      const __m128i clprod22 = _mm_clmulepi64_si128_emu(temp22, temp22, 0x10);
      acc = _mm_xor_si128_emu(clprod22, acc);

      const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
      const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
      _mm_store_si128_emu(prandex, tempb2);
      break;
    }
    case 0xc: {
      const __m128i temp1 = _mm_load_si128_emu(prand);
      const __m128i temp2 =
          _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);

      // cannot be zero here
      const int32_t divisor = (uint32_t)selector;

      acc = _mm_xor_si128_emu(add1, acc);

      const int64_t dividend = _mm_cvtsi128_si64_emu(acc);
      const __m128i modulo = _mm_cvtsi32_si128_emu(dividend % divisor);
      acc = _mm_xor_si128_emu(modulo, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

      if (dividend & 1) {
        const __m128i temp12 = _mm_load_si128_emu(prandex);
        _mm_store_si128_emu(prandex, tempa2);

        const __m128i temp22 = _mm_load_si128_emu(pbuf);
        const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
        const __m128i clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
        acc = _mm_xor_si128_emu(clprod12, acc);
        const __m128i clprod22 = _mm_clmulepi64_si128_emu(temp22, temp22, 0x10);
        acc = _mm_xor_si128_emu(clprod22, acc);

        const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
        const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
        _mm_store_si128_emu(prand, tempb2);
      } else {
        const __m128i tempb3 = _mm_load_si128_emu(prandex);
        _mm_store_si128_emu(prandex, tempa2);
        _mm_store_si128_emu(prand, tempb3);
      }
      break;
    }
    case 0x10: {
      // a few AES operations
      const __m128i *rc = prand;
      __m128i tmp;

      __m128i temp1 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      __m128i temp2 = _mm_load_si128_emu(pbuf);

      AES2_EMU(temp1, temp2, 0);
      MIX2_EMU(temp1, temp2);

      AES2_EMU(temp1, temp2, 4);
      MIX2_EMU(temp1, temp2);

      AES2_EMU(temp1, temp2, 8);
      MIX2_EMU(temp1, temp2);

      acc = _mm_xor_si128_emu(temp1, acc);
      acc = _mm_xor_si128_emu(temp2, acc);

      const __m128i tempa1 = _mm_load_si128_emu(prand);
      const __m128i tempa2 = _mm_mulhrs_epi16_emu(acc, tempa1);
      const __m128i tempa3 = _mm_xor_si128_emu(tempa1, tempa2);

      const __m128i tempa4 = _mm_load_si128_emu(prandex);
      _mm_store_si128_emu(prandex, tempa3);
      _mm_store_si128_emu(prand, tempa4);
      break;
    }
    case 0x14: {
      // we'll just call this one the monkins loop, inspired by Chris
      const __m128i *buftmp = pbuf - (((selector & 1) << 1) - 1);
      __m128i tmp; // used by MIX2

      uint64_t rounds = selector >> 61; // loop randomly between 1 and 8 times
      __m128i *rc = prand;
      uint64_t aesround = 0;
      __m128i onekey;

      do {
        // this is simplified over the original verus_clhash
        if (selector & (((uint64_t)0x10000000) << rounds)) {
          onekey = _mm_load_si128_emu(rc++);
          const __m128i temp2 = _mm_load_si128_emu(rounds & 1 ? pbuf : buftmp);
          const __m128i add1 = _mm_xor_si128_emu(onekey, temp2);
          const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
          acc = _mm_xor_si128_emu(clprod1, acc);
        } else {
          onekey = _mm_load_si128_emu(rc++);
          __m128i temp2 = _mm_load_si128_emu(rounds & 1 ? buftmp : pbuf);
          const uint64_t roundidx = aesround++ << 2;
          AES2_EMU(onekey, temp2, roundidx);

          MIX2_EMU(onekey, temp2);

          acc = _mm_xor_si128_emu(onekey, acc);
          acc = _mm_xor_si128_emu(temp2, acc);
        }
      } while (rounds--);

      const __m128i tempa1 = _mm_load_si128_emu(prand);
      const __m128i tempa2 = _mm_mulhrs_epi16_emu(acc, tempa1);
      const __m128i tempa3 = _mm_xor_si128_emu(tempa1, tempa2);

      const __m128i tempa4 = _mm_load_si128_emu(prandex);
      _mm_store_si128_emu(prandex, tempa3);
      _mm_store_si128_emu(prand, tempa4);
      break;
    }
    case 0x18: {
      const __m128i *buftmp = pbuf - (((selector & 1) << 1) - 1);

      uint64_t rounds = selector >> 61; // loop randomly between 1 and 8 times
      __m128i *rc = prand;
      __m128i onekey;

      do {
        if (selector & (((uint64_t)0x10000000) << rounds)) {
          onekey = _mm_load_si128_emu(rc++);
          const __m128i temp2 = _mm_load_si128_emu(rounds & 1 ? pbuf : buftmp);
          const __m128i add1 = _mm_xor_si128_emu(onekey, temp2);
          // cannot be zero here, may be negative
          const int32_t divisor = (uint32_t)selector;
          const int64_t dividend = _mm_cvtsi128_si64_emu(add1);
          const __m128i modulo = _mm_cvtsi32_si128_emu(dividend % divisor);
          acc = _mm_xor_si128_emu(modulo, acc);
        } else {
          onekey = _mm_load_si128_emu(rc++);
          __m128i temp2 = _mm_load_si128_emu(rounds & 1 ? buftmp : pbuf);
          const __m128i add1 = _mm_xor_si128_emu(onekey, temp2);
          const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
          const __m128i clprod2 = _mm_mulhrs_epi16_emu(acc, clprod1);
          acc = _mm_xor_si128_emu(clprod2, acc);
        }
      } while (rounds--);

      const __m128i tempa3 = _mm_load_si128_emu(prandex);
      const __m128i tempa4 = _mm_xor_si128_emu(tempa3, acc);
      _mm_store_si128_emu(prandex, tempa4);
      _mm_store_si128_emu(prand, onekey);
      break;
    }
    case 0x1c: {
      const __m128i temp1 = _mm_load_si128_emu(pbuf);
      const __m128i temp2 = _mm_load_si128_emu(prandex);
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
      const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
      acc = _mm_xor_si128_emu(clprod1, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp2);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp2);

      const __m128i tempa3 = _mm_load_si128_emu(prand);
      _mm_store_si128_emu(prand, tempa2);

      acc = _mm_xor_si128_emu(tempa3, acc);
      const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, tempa3);
      const __m128i tempb2 = _mm_xor_si128_emu(tempb1, tempa3);
      _mm_store_si128_emu(prandex, tempb2);
      break;
    }
    }
  }
  return acc;
}

// verus intermediate hash extra
__m128i __verusclmulwithoutreduction64alignedrepeat_sv2_2_port(
    __m128i *randomsource, const __m128i buf[4], uint64_t keyMask,
    __m128i **pMoveScratch) {
  const __m128i pbuf_copy[4] = {_mm_xor_si128(buf[0], buf[2]),
                                _mm_xor_si128(buf[1], buf[3]), buf[2], buf[3]};
  const __m128i *pbuf;

  // divide key mask by 16 from bytes to __m128i
  keyMask >>= 4;

  // the random buffer must have at least 32 16 byte dwords after the keymask to
  // work with this algorithm. we take the value from the last element inside
  // the keyMask + 2, as that will never be used to xor into the accumulator
  // before it is hashed with other values first
  __m128i acc = _mm_load_si128_emu(randomsource + (keyMask + 2));

  for (int64_t i = 0; i < 32; i++) {
    // std::cout << "LOOP " << i << " acc: " << LEToHex(acc) << std::endl;

    const uint64_t selector = _mm_cvtsi128_si64_emu(acc);

    // get two random locations in the key, which will be mutated and swapped
    __m128i *prand = randomsource + ((selector >> 5) & keyMask);
    __m128i *prandex = randomsource + ((selector >> 32) & keyMask);

    *pMoveScratch++ = prand;
    *pMoveScratch++ = prandex;

    // select random start and order of pbuf processing
    pbuf = pbuf_copy + (selector & 3);

    switch (selector & 0x1c) {
    case 0: {
      const __m128i temp1 = _mm_load_si128_emu(prandex);
      const __m128i temp2 =
          _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
      const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
      acc = _mm_xor_si128_emu(clprod1, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

      const __m128i temp12 = _mm_load_si128_emu(prand);
      _mm_store_si128_emu(prand, tempa2);

      const __m128i temp22 = _mm_load_si128_emu(pbuf);
      const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
      const __m128i clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
      acc = _mm_xor_si128_emu(clprod12, acc);

      const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
      const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
      _mm_store_si128_emu(prandex, tempb2);
      break;
    }
    case 4: {
      const __m128i temp1 = _mm_load_si128_emu(prand);
      const __m128i temp2 = _mm_load_si128_emu(pbuf);
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
      const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
      acc = _mm_xor_si128_emu(clprod1, acc);
      const __m128i clprod2 = _mm_clmulepi64_si128_emu(temp2, temp2, 0x10);
      acc = _mm_xor_si128_emu(clprod2, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

      const __m128i temp12 = _mm_load_si128_emu(prandex);
      _mm_store_si128_emu(prandex, tempa2);

      const __m128i temp22 =
          _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
      acc = _mm_xor_si128_emu(add12, acc);

      const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
      const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
      _mm_store_si128_emu(prand, tempb2);
      break;
    }
    case 8: {
      const __m128i temp1 = _mm_load_si128_emu(prandex);
      const __m128i temp2 = _mm_load_si128_emu(pbuf);
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
      acc = _mm_xor_si128_emu(add1, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

      const __m128i temp12 = _mm_load_si128_emu(prand);
      _mm_store_si128_emu(prand, tempa2);

      const __m128i temp22 =
          _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
      const __m128i clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
      acc = _mm_xor_si128_emu(clprod12, acc);
      const __m128i clprod22 = _mm_clmulepi64_si128_emu(temp22, temp22, 0x10);
      acc = _mm_xor_si128_emu(clprod22, acc);

      const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
      const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
      _mm_store_si128_emu(prandex, tempb2);
      break;
    }
    case 0xc: {
      const __m128i temp1 = _mm_load_si128_emu(prand);
      const __m128i temp2 =
          _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);

      // cannot be zero here
      const int32_t divisor = (uint32_t)selector;

      acc = _mm_xor_si128_emu(add1, acc);

      const int64_t dividend = _mm_cvtsi128_si64_emu(acc);
      const __m128i modulo = _mm_cvtsi32_si128_emu(dividend % divisor);
      acc = _mm_xor_si128_emu(modulo, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp1);

      if (dividend & 1) {
        const __m128i temp12 = _mm_load_si128_emu(prandex);
        _mm_store_si128_emu(prandex, tempa2);

        const __m128i temp22 = _mm_load_si128_emu(pbuf);
        const __m128i add12 = _mm_xor_si128_emu(temp12, temp22);
        const __m128i clprod12 = _mm_clmulepi64_si128_emu(add12, add12, 0x10);
        acc = _mm_xor_si128_emu(clprod12, acc);
        const __m128i clprod22 = _mm_clmulepi64_si128_emu(temp22, temp22, 0x10);
        acc = _mm_xor_si128_emu(clprod22, acc);

        const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, temp12);
        const __m128i tempb2 = _mm_xor_si128_emu(tempb1, temp12);
        _mm_store_si128_emu(prand, tempb2);
      } else {
        const __m128i tempb3 = _mm_load_si128_emu(prandex);
        _mm_store_si128_emu(prandex, tempa2);
        _mm_store_si128_emu(prand, tempb3);
        const __m128i tempb4 = _mm_load_si128_emu(pbuf);
        acc = _mm_xor_si128_emu(tempb4, acc);
      }
      break;
    }
    case 0x10: {
      // a few AES operations
      const __m128i *rc = prand;
      __m128i tmp;

      __m128i temp1 = _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      __m128i temp2 = _mm_load_si128_emu(pbuf);

      AES2_EMU(temp1, temp2, 0);
      MIX2_EMU(temp1, temp2);

      AES2_EMU(temp1, temp2, 4);
      MIX2_EMU(temp1, temp2);

      AES2_EMU(temp1, temp2, 8);
      MIX2_EMU(temp1, temp2);

      acc = _mm_xor_si128_emu(temp1, acc);
      acc = _mm_xor_si128_emu(temp2, acc);

      const __m128i tempa1 = _mm_load_si128_emu(prand);
      const __m128i tempa2 = _mm_mulhrs_epi16_emu(acc, tempa1);
      const __m128i tempa3 = _mm_xor_si128_emu(tempa1, tempa2);

      const __m128i tempa4 = _mm_load_si128_emu(prandex);
      _mm_store_si128_emu(prandex, tempa3);
      _mm_store_si128_emu(prand, tempa4);
      break;
    }
    case 0x14: {
      // we'll just call this one the monkins loop, inspired by Chris
      const __m128i *buftmp = pbuf - (((selector & 1) << 1) - 1);
      __m128i tmp; // used by MIX2

      uint64_t rounds = selector >> 61; // loop randomly between 1 and 8 times
      __m128i *rc = prand;
      uint64_t aesround = 0;
      __m128i onekey;

      do {
        // this is simplified over the original verus_clhash
        if (selector & (((uint64_t)0x10000000) << rounds)) {
          onekey = _mm_load_si128_emu(rc++);
          const __m128i temp2 = _mm_load_si128_emu(rounds & 1 ? pbuf : buftmp);
          const __m128i add1 = _mm_xor_si128_emu(onekey, temp2);
          const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
          acc = _mm_xor_si128_emu(clprod1, acc);
        } else {
          onekey = _mm_load_si128_emu(rc++);
          __m128i temp2 = _mm_load_si128_emu(rounds & 1 ? buftmp : pbuf);
          const uint64_t roundidx = aesround++ << 2;
          AES2_EMU(onekey, temp2, roundidx);

          MIX2_EMU(onekey, temp2);

          acc = _mm_xor_si128_emu(onekey, acc);
          acc = _mm_xor_si128_emu(temp2, acc);
        }
      } while (rounds--);

      const __m128i tempa1 = _mm_load_si128_emu(prand);
      const __m128i tempa2 = _mm_mulhrs_epi16_emu(acc, tempa1);
      const __m128i tempa3 = _mm_xor_si128_emu(tempa1, tempa2);

      const __m128i tempa4 = _mm_load_si128_emu(prandex);
      _mm_store_si128_emu(prandex, tempa3);
      _mm_store_si128_emu(prand, tempa4);
      break;
    }
    case 0x18: {
      const __m128i *buftmp = pbuf - (((selector & 1) << 1) - 1);

      uint64_t rounds = selector >> 61; // loop randomly between 1 and 8 times
      __m128i *rc = prand;
      __m128i onekey;

      do {
        if (selector & (((uint64_t)0x10000000) << rounds)) {
          onekey = _mm_load_si128_emu(rc++);
          const __m128i temp2 = _mm_load_si128_emu(rounds & 1 ? pbuf : buftmp);
          onekey = _mm_xor_si128_emu(onekey, temp2);
          // cannot be zero here, may be negative
          const int32_t divisor = (uint32_t)selector;
          const int64_t dividend = _mm_cvtsi128_si64_emu(onekey);
          const __m128i modulo = _mm_cvtsi32_si128_emu(dividend % divisor);
          acc = _mm_xor_si128_emu(modulo, acc);
        } else {
          onekey = _mm_load_si128_emu(rc++);
          __m128i temp2 = _mm_load_si128_emu(rounds & 1 ? buftmp : pbuf);
          const __m128i add1 = _mm_xor_si128_emu(onekey, temp2);
          onekey = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
          const __m128i clprod2 = _mm_mulhrs_epi16_emu(acc, onekey);
          acc = _mm_xor_si128_emu(clprod2, acc);
        }
      } while (rounds--);

      const __m128i tempa3 = _mm_load_si128_emu(prandex);
      const __m128i tempa4 = _mm_xor_si128_emu(tempa3, acc);
      _mm_store_si128_emu(prandex, onekey);
      _mm_store_si128_emu(prand, tempa4);
      break;
    }
    case 0x1c: {
      const __m128i temp1 = _mm_load_si128_emu(pbuf);
      const __m128i temp2 = _mm_load_si128_emu(prandex);
      const __m128i add1 = _mm_xor_si128_emu(temp1, temp2);
      const __m128i clprod1 = _mm_clmulepi64_si128_emu(add1, add1, 0x10);
      acc = _mm_xor_si128_emu(clprod1, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16_emu(acc, temp2);
      const __m128i tempa2 = _mm_xor_si128_emu(tempa1, temp2);

      const __m128i tempa3 = _mm_load_si128_emu(prand);
      _mm_store_si128_emu(prand, tempa2);

      acc = _mm_xor_si128_emu(tempa3, acc);
      const __m128i temp4 =
          _mm_load_si128_emu(pbuf - (((selector & 1) << 1) - 1));
      acc = _mm_xor_si128_emu(temp4, acc);
      const __m128i tempb1 = _mm_mulhrs_epi16_emu(acc, tempa3);
      const __m128i tempb2 = _mm_xor_si128_emu(tempb1, tempa3);
      _mm_store_si128_emu(prandex, tempb2);
      break;
    }
    }
  }
  return acc;
}

// hashes 64 bytes only by doing a carryless multiplication and reduction of the
// repeated 64 byte sequence 16 times, returning a 64 bit hash value
uint64_t verusclhash_port(void *random, const unsigned char buf[64],
                          uint64_t keyMask, __m128i **pMoveScratch) {
  __m128i *rs64 = (__m128i *)random;
  const __m128i *string = (const __m128i *)buf;

  __m128i acc = __verusclmulwithoutreduction64alignedrepeat_port(
      rs64, string, keyMask, pMoveScratch);
  acc = _mm_xor_si128_emu(acc, lazyLengthHash_port(1024, 64));
  return precompReduction64_port(acc);
}

// hashes 64 bytes only by doing a carryless multiplication and reduction of the
// repeated 64 byte sequence 16 times, returning a 64 bit hash value
uint64_t verusclhash_sv2_1_port(void *random, const unsigned char buf[64],
                                uint64_t keyMask, __m128i **pMoveScratch) {
  __m128i *rs64 = (__m128i *)random;
  const __m128i *string = (const __m128i *)buf;

  __m128i acc = __verusclmulwithoutreduction64alignedrepeat_sv2_1_port(
      rs64, string, keyMask, pMoveScratch);
  acc = _mm_xor_si128_emu(acc, lazyLengthHash_port(1024, 64));
  return precompReduction64_port(acc);
}

uint64_t verusclhash_sv2_2_port(void *random, const unsigned char buf[64],
                                uint64_t keyMask, __m128i **pMoveScratch) {
  __m128i *rs64 = (__m128i *)random;
  const __m128i *string = (const __m128i *)buf;

  __m128i acc = __verusclmulwithoutreduction64alignedrepeat_sv2_2_port(
      rs64, string, keyMask, pMoveScratch);
  acc = _mm_xor_si128_emu(acc, lazyLengthHash_port(1024, 64));
  return precompReduction64_port(acc);
}

