#include "verus_clhash_opt.hpp"
#include <iostream>

#ifdef _WIN32
#pragma warning(disable : 4146)
#include <intrin.h>
#endif

#if defined(__arm__) || defined(__aarch64__)
#include "crypto/sse2neon.h"
#else
#include <x86intrin.h>
#endif

namespace {

#define LOAD(src) _mm_load_si128((u128 *)(src))
#define STORE(dest, src) _mm_storeu_si128((u128 *)(dest), src)

#define AES2(s0, s1, rci)                                                      \
  s0 = _mm_aesenc_si128(s0, rc[rci]);                                          \
  s1 = _mm_aesenc_si128(s1, rc[rci + 1]);                                      \
  s0 = _mm_aesenc_si128(s0, rc[rci + 2]);                                      \
  s1 = _mm_aesenc_si128(s1, rc[rci + 3]);

#define AES2_4x(s0, s1, s2, s3, rci)                                           \
  AES2(s0[0], s0[1], rci);                                                     \
  AES2(s1[0], s1[1], rci);                                                     \
  AES2(s2[0], s2[1], rci);                                                     \
  AES2(s3[0], s3[1], rci);

#define AES2_8x(s0, s1, s2, s3, s4, s5, s6, s7, rci)                           \
  AES2_4x(s0, s1, s2, s3, rci);                                                \
  AES2_4x(s4, s5, s6, s7, rci);

#define AES4(s0, s1, s2, s3, rci)                                              \
  s0 = _mm_aesenc_si128(s0, rc[rci]);                                          \
  s1 = _mm_aesenc_si128(s1, rc[rci + 1]);                                      \
  s2 = _mm_aesenc_si128(s2, rc[rci + 2]);                                      \
  s3 = _mm_aesenc_si128(s3, rc[rci + 3]);                                      \
  s0 = _mm_aesenc_si128(s0, rc[rci + 4]);                                      \
  s1 = _mm_aesenc_si128(s1, rc[rci + 5]);                                      \
  s2 = _mm_aesenc_si128(s2, rc[rci + 6]);                                      \
  s3 = _mm_aesenc_si128(s3, rc[rci + 7]);

#define AES4_zero(s0, s1, s2, s3, rci)                                         \
  s0 = _mm_aesenc_si128(s0, rc0[rci]);                                         \
  s1 = _mm_aesenc_si128(s1, rc0[rci + 1]);                                     \
  s2 = _mm_aesenc_si128(s2, rc0[rci + 2]);                                     \
  s3 = _mm_aesenc_si128(s3, rc0[rci + 3]);                                     \
  s0 = _mm_aesenc_si128(s0, rc0[rci + 4]);                                     \
  s1 = _mm_aesenc_si128(s1, rc0[rci + 5]);                                     \
  s2 = _mm_aesenc_si128(s2, rc0[rci + 6]);                                     \
  s3 = _mm_aesenc_si128(s3, rc0[rci + 7]);

#define AES4_4x(s0, s1, s2, s3, rci)                                           \
  AES4(s0[0], s0[1], s0[2], s0[3], rci);                                       \
  AES4(s1[0], s1[1], s1[2], s1[3], rci);                                       \
  AES4(s2[0], s2[1], s2[2], s2[3], rci);                                       \
  AES4(s3[0], s3[1], s3[2], s3[3], rci);

#define AES4_8x(s0, s1, s2, s3, s4, s5, s6, s7, rci)                           \
  AES4_4x(s0, s1, s2, s3, rci);                                                \
  AES4_4x(s4, s5, s6, s7, rci);

#define MIX2(s0, s1)                                                           \
  tmp = _mm_unpacklo_epi32(s0, s1);                                            \
  s1 = _mm_unpackhi_epi32(s0, s1);                                             \
  s0 = tmp;

#define MIX4(s0, s1, s2, s3)                                                   \
  tmp = _mm_unpacklo_epi32(s0, s1);                                            \
  s0 = _mm_unpackhi_epi32(s0, s1);                                             \
  s1 = _mm_unpacklo_epi32(s2, s3);                                             \
  s2 = _mm_unpackhi_epi32(s2, s3);                                             \
  s3 = _mm_unpacklo_epi32(s0, s2);                                             \
  s0 = _mm_unpackhi_epi32(s0, s2);                                             \
  s2 = _mm_unpackhi_epi32(s1, tmp);                                            \
  s1 = _mm_unpacklo_epi32(s1, tmp);

#define TRUNCSTORE(out, s0, s1, s2, s3)                                        \
  *(u64 *)(out) = *(((u64 *)&s0 + 1));                                         \
  *(u64 *)(out + 8) = *(((u64 *)&s1 + 1));                                     \
  *(u64 *)(out + 16) = *(((u64 *)&s2 + 0));                                    \
  *(u64 *)(out + 24) = *(((u64 *)&s3 + 0));

static const unsigned char rc_raw[40][16] = {
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
const u128 *rc = (const u128 *)rc_raw;
} // namespace

void haraka256(unsigned char *out, const unsigned char *in) {
  __m128i s[2], tmp;

  s[0] = LOAD(in);
  s[1] = LOAD(in + 16);

  AES2(s[0], s[1], 0);
  MIX2(s[0], s[1]);

  AES2(s[0], s[1], 4);
  MIX2(s[0], s[1]);

  AES2(s[0], s[1], 8);
  MIX2(s[0], s[1]);

  AES2(s[0], s[1], 12);
  MIX2(s[0], s[1]);

  AES2(s[0], s[1], 16);
  MIX2(s[0], s[1]);

  s[0] = _mm_xor_si128(s[0], LOAD(in));
  s[1] = _mm_xor_si128(s[1], LOAD(in + 16));

  STORE(out, s[0]);
  STORE(out + 16, s[1]);
}

void haraka512(unsigned char *out, const unsigned char *in) {
  u128 s[4], tmp;

  s[0] = LOAD(in);
  s[1] = LOAD(in + 16);
  s[2] = LOAD(in + 32);
  s[3] = LOAD(in + 48);

  AES4(s[0], s[1], s[2], s[3], 0);
  MIX4(s[0], s[1], s[2], s[3]);

  AES4(s[0], s[1], s[2], s[3], 8);
  MIX4(s[0], s[1], s[2], s[3]);

  AES4(s[0], s[1], s[2], s[3], 16);
  MIX4(s[0], s[1], s[2], s[3]);

  AES4(s[0], s[1], s[2], s[3], 24);
  MIX4(s[0], s[1], s[2], s[3]);

  AES4(s[0], s[1], s[2], s[3], 32);
  MIX4(s[0], s[1], s[2], s[3]);

  s[0] = _mm_xor_si128(s[0], LOAD(in));
  s[1] = _mm_xor_si128(s[1], LOAD(in + 16));
  s[2] = _mm_xor_si128(s[2], LOAD(in + 32));
  s[3] = _mm_xor_si128(s[3], LOAD(in + 48));

  TRUNCSTORE(out, s[0], s[1], s[2], s[3]);
}

void haraka512_keyed(unsigned char *out, const unsigned char *in,
                     const u128 *rc) {
  u128 s[4], tmp;

  s[0] = LOAD(in);
  s[1] = LOAD(in + 16);
  s[2] = LOAD(in + 32);
  s[3] = LOAD(in + 48);

  AES4(s[0], s[1], s[2], s[3], 0);
  MIX4(s[0], s[1], s[2], s[3]);

  AES4(s[0], s[1], s[2], s[3], 8);
  MIX4(s[0], s[1], s[2], s[3]);

  AES4(s[0], s[1], s[2], s[3], 16);
  MIX4(s[0], s[1], s[2], s[3]);

  AES4(s[0], s[1], s[2], s[3], 24);
  MIX4(s[0], s[1], s[2], s[3]);

  AES4(s[0], s[1], s[2], s[3], 32);
  MIX4(s[0], s[1], s[2], s[3]);

  s[0] = _mm_xor_si128(s[0], LOAD(in));
  s[1] = _mm_xor_si128(s[1], LOAD(in + 16));
  s[2] = _mm_xor_si128(s[2], LOAD(in + 32));
  s[3] = _mm_xor_si128(s[3], LOAD(in + 48));

  TRUNCSTORE(out, s[0], s[1], s[2], s[3]);
}

namespace {

// multiply the length and the some key, no modulo
static inline __attribute__((always_inline)) __m128i
lazyLengthHash(uint64_t keylength, uint64_t length) {

  const __m128i lengthvector = _mm_set_epi64x(keylength, length);
  const __m128i clprod1 =
      _mm_clmulepi64_si128(lengthvector, lengthvector, 0x10);
  return clprod1;
}

// modulo reduction to 64-bit value. The high 64 bits contain garbage, see
// precompReduction64
static inline __attribute__((always_inline)) __m128i
precompReduction64_si128(__m128i A) {
  // const __m128i C = _mm_set_epi64x(1U,(1U<<4)+(1U<<3)+(1U<<1)+(1U<<0)); // C
  // is the irreducible poly. (64,4,3,1,0)
  const __m128i C =
      _mm_cvtsi64_si128((1U << 4) + (1U << 3) + (1U << 1) + (1U << 0));
  __m128i Q2 = _mm_clmulepi64_si128(A, C, 0x01);
  __m128i Q3 =
      _mm_shuffle_epi8(_mm_setr_epi8(0, 27, 54, 45, 108, 119, 90, 65, (char)216,
                                     (char)195, (char)238, (char)245, (char)180,
                                     (char)175, (char)130, (char)153),
                       _mm_srli_si128(Q2, 8));
  __m128i Q4 = _mm_xor_si128(Q2, A);
  const __m128i final = _mm_xor_si128(Q3, Q4);
  return final; /// WARNING: HIGH 64 BITS CONTAIN GARBAGE
}

static inline __attribute__((always_inline)) uint64_t
precompReduction64(__m128i A) {
  return _mm_cvtsi128_si64(precompReduction64_si128(A));
}

static inline __attribute__((always_inline)) void
haraka512_keyed_local(unsigned char *out, const unsigned char *in,
                      const u128 *rc) {
  u128 s[4], tmp;

  s[0] = LOAD(in);
  s[1] = LOAD(in + 16);
  s[2] = LOAD(in + 32);
  s[3] = LOAD(in + 48);

  AES4(s[0], s[1], s[2], s[3], 0);
  MIX4(s[0], s[1], s[2], s[3]);

  AES4(s[0], s[1], s[2], s[3], 8);
  MIX4(s[0], s[1], s[2], s[3]);

  AES4(s[0], s[1], s[2], s[3], 16);
  MIX4(s[0], s[1], s[2], s[3]);

  AES4(s[0], s[1], s[2], s[3], 24);
  MIX4(s[0], s[1], s[2], s[3]);

  AES4(s[0], s[1], s[2], s[3], 32);
  MIX4(s[0], s[1], s[2], s[3]);

  s[0] = _mm_xor_si128(s[0], LOAD(in));
  s[1] = _mm_xor_si128(s[1], LOAD(in + 16));
  s[2] = _mm_xor_si128(s[2], LOAD(in + 32));
  s[3] = _mm_xor_si128(s[3], LOAD(in + 48));

  TRUNCSTORE(out, s[0], s[1], s[2], s[3]);
}

// verus intermediate hash extra
// __m128i __verusclmulwithoutreduction64alignedrepeat(__m128i *randomsource,
//                                                     const __m128i buf[4],
//                                                     uint64_t keyMask,
//                                                     __m128i **pMoveScratch) {
//   __m128i const *pbuf;
//
//   // divide key mask by 16 from bytes to __m128i
//   keyMask >>= 4;
//
//   // the random buffer must have at least 32 16 byte dwords after the keymask
//   to
//   // work with this algorithm. we take the value from the last element inside
//   // the keyMask + 2, as that will never be used to xor into the accumulator
//   // before it is hashed with other values first
//   __m128i acc = _mm_load_si128(randomsource + (keyMask + 2));
//
//   for (int64_t i = 0; i < 32; i++) {
//     const uint64_t selector = _mm_cvtsi128_si64(acc);
//
//     // get two random locations in the key, which will be mutated and swapped
//     __m128i *prand = randomsource + ((selector >> 5) & keyMask);
//     __m128i *prandex = randomsource + ((selector >> 32) & keyMask);
//
//     *(pMoveScratch++) = prand;
//     *(pMoveScratch++) = prandex;
//
//     // select random start and order of pbuf processing
//     pbuf = buf + (selector & 3);
//
//     switch (selector & 0x1c) {
//     case 0: {
//       const __m128i temp1 = _mm_load_si128(prandex);
//       const __m128i temp2 = _mm_load_si128(pbuf - (((selector & 1) << 1) -
//       1)); const __m128i add1 = _mm_xor_si128(temp1, temp2); const __m128i
//       clprod1 = _mm_clmulepi64_si128(add1, add1, 0x10); acc =
//       _mm_xor_si128(clprod1, acc);
//
//       const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp1);
//       const __m128i tempa2 = _mm_xor_si128(tempa1, temp1);
//
//       const __m128i temp12 = _mm_load_si128(prand);
//       _mm_store_si128(prand, tempa2);
//
//       const __m128i temp22 = _mm_load_si128(pbuf);
//       const __m128i add12 = _mm_xor_si128(temp12, temp22);
//       const __m128i clprod12 = _mm_clmulepi64_si128(add12, add12, 0x10);
//       acc = _mm_xor_si128(clprod12, acc);
//
//       const __m128i tempb1 = _mm_mulhrs_epi16(acc, temp12);
//       const __m128i tempb2 = _mm_xor_si128(tempb1, temp12);
//       _mm_store_si128(prandex, tempb2);
//       break;
//     }
//     case 4: {
//       const __m128i temp1 = _mm_load_si128(prand);
//       const __m128i temp2 = _mm_load_si128(pbuf);
//       const __m128i add1 = _mm_xor_si128(temp1, temp2);
//       const __m128i clprod1 = _mm_clmulepi64_si128(add1, add1, 0x10);
//       acc = _mm_xor_si128(clprod1, acc);
//       const __m128i clprod2 = _mm_clmulepi64_si128(temp2, temp2, 0x10);
//       acc = _mm_xor_si128(clprod2, acc);
//
//       const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp1);
//       const __m128i tempa2 = _mm_xor_si128(tempa1, temp1);
//
//       const __m128i temp12 = _mm_load_si128(prandex);
//       _mm_store_si128(prandex, tempa2);
//
//       const __m128i temp22 = _mm_load_si128(pbuf - (((selector & 1) << 1) -
//       1)); const __m128i add12 = _mm_xor_si128(temp12, temp22); acc =
//       _mm_xor_si128(add12, acc);
//
//       const __m128i tempb1 = _mm_mulhrs_epi16(acc, temp12);
//       const __m128i tempb2 = _mm_xor_si128(tempb1, temp12);
//       _mm_store_si128(prand, tempb2);
//       break;
//     }
//     case 8: {
//       const __m128i temp1 = _mm_load_si128(prandex);
//       const __m128i temp2 = _mm_load_si128(pbuf);
//       const __m128i add1 = _mm_xor_si128(temp1, temp2);
//       acc = _mm_xor_si128(add1, acc);
//
//       const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp1);
//       const __m128i tempa2 = _mm_xor_si128(tempa1, temp1);
//
//       const __m128i temp12 = _mm_load_si128(prand);
//       _mm_store_si128(prand, tempa2);
//
//       const __m128i temp22 = _mm_load_si128(pbuf - (((selector & 1) << 1) -
//       1)); const __m128i add12 = _mm_xor_si128(temp12, temp22); const __m128i
//       clprod12 = _mm_clmulepi64_si128(add12, add12, 0x10); acc =
//       _mm_xor_si128(clprod12, acc); const __m128i clprod22 =
//       _mm_clmulepi64_si128(temp22, temp22, 0x10); acc =
//       _mm_xor_si128(clprod22, acc);
//
//       const __m128i tempb1 = _mm_mulhrs_epi16(acc, temp12);
//       const __m128i tempb2 = _mm_xor_si128(tempb1, temp12);
//       _mm_store_si128(prandex, tempb2);
//       break;
//     }
//     case 0xc: {
//       const __m128i temp1 = _mm_load_si128(prand);
//       const __m128i temp2 = _mm_load_si128(pbuf - (((selector & 1) << 1) -
//       1)); const __m128i add1 = _mm_xor_si128(temp1, temp2);
//
//       // cannot be zero here
//       const int32_t divisor = (uint32_t)selector;
//
//       acc = _mm_xor_si128(add1, acc);
//
//       const int64_t dividend = _mm_cvtsi128_si64(acc);
//       const __m128i modulo = _mm_cvtsi32_si128(dividend % divisor);
//       acc = _mm_xor_si128(modulo, acc);
//
//       const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp1);
//       const __m128i tempa2 = _mm_xor_si128(tempa1, temp1);
//
//       if (dividend & 1) {
//         const __m128i temp12 = _mm_load_si128(prandex);
//         _mm_store_si128(prandex, tempa2);
//
//         const __m128i temp22 = _mm_load_si128(pbuf);
//         const __m128i add12 = _mm_xor_si128(temp12, temp22);
//         const __m128i clprod12 = _mm_clmulepi64_si128(add12, add12, 0x10);
//         acc = _mm_xor_si128(clprod12, acc);
//         const __m128i clprod22 = _mm_clmulepi64_si128(temp22, temp22, 0x10);
//         acc = _mm_xor_si128(clprod22, acc);
//
//         const __m128i tempb1 = _mm_mulhrs_epi16(acc, temp12);
//         const __m128i tempb2 = _mm_xor_si128(tempb1, temp12);
//         _mm_store_si128(prand, tempb2);
//       } else {
//         const __m128i tempb3 = _mm_load_si128(prandex);
//         _mm_store_si128(prandex, tempa2);
//         _mm_store_si128(prand, tempb3);
//       }
//       break;
//     }
//     case 0x10: {
//       // a few AES operations
//       __m128i tmp;
//
//       __m128i temp1 = _mm_load_si128(pbuf - (((selector & 1) << 1) - 1));
//       __m128i temp2 = _mm_load_si128(pbuf);
//
//       AES2(temp1, temp2, 0);
//       MIX2(temp1, temp2);
//
//       AES2(temp1, temp2, 4);
//       MIX2(temp1, temp2);
//
//       AES2(temp1, temp2, 8);
//       MIX2(temp1, temp2);
//
//       acc = _mm_xor_si128(temp2, _mm_xor_si128(temp1, acc));
//
//       const __m128i tempa1 = _mm_load_si128(prand);
//       const __m128i tempa2 = _mm_mulhrs_epi16(acc, tempa1);
//       const __m128i tempa3 = _mm_xor_si128(tempa1, tempa2);
//
//       const __m128i tempa4 = _mm_load_si128(prandex);
//       _mm_store_si128(prandex, tempa3);
//       _mm_store_si128(prand, tempa4);
//       break;
//     }
//     case 0x14: {
//       // we'll just call this one the monkins loop, inspired by Chris
//       const __m128i *buftmp = pbuf - (((selector & 1) << 1) - 1);
//       __m128i tmp; // used by MIX2
//
//       uint64_t rounds = selector >> 61; // loop randomly between 1 and 8
//       times
//       __m128i *rc = prand;
//       uint64_t aesroundoffset = 0;
//       __m128i onekey;
//
//       do {
//         if (selector & (0x10000000 << rounds)) {
//           onekey = _mm_load_si128(rc++);
//           const __m128i temp2 = _mm_load_si128(rounds & 1 ? pbuf : buftmp);
//           const __m128i add1 = _mm_xor_si128(onekey, temp2);
//           const __m128i clprod1 = _mm_clmulepi64_si128(add1, add1, 0x10);
//           acc = _mm_xor_si128(clprod1, acc);
//         } else {
//           onekey = _mm_load_si128(rc++);
//           __m128i temp2 = _mm_load_si128(rounds & 1 ? buftmp : pbuf);
//           AES2(onekey, temp2, aesroundoffset);
//           aesroundoffset += 4;
//           MIX2(onekey, temp2);
//           acc = _mm_xor_si128(onekey, acc);
//           acc = _mm_xor_si128(temp2, acc);
//         }
//       } while (rounds--);
//
//       const __m128i tempa1 = _mm_load_si128(prand);
//       const __m128i tempa2 = _mm_mulhrs_epi16(acc, tempa1);
//       const __m128i tempa3 = _mm_xor_si128(tempa1, tempa2);
//
//       const __m128i tempa4 = _mm_load_si128(prandex);
//       _mm_store_si128(prandex, tempa3);
//       _mm_store_si128(prand, tempa4);
//       break;
//     }
//     case 0x18: {
//       const __m128i temp1 = _mm_load_si128(pbuf - (((selector & 1) << 1) -
//       1)); const __m128i temp2 = _mm_load_si128(prand); const __m128i add1 =
//       _mm_xor_si128(temp1, temp2); const __m128i clprod1 =
//       _mm_clmulepi64_si128(add1, add1, 0x10); acc = _mm_xor_si128(clprod1,
//       acc);
//
//       const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp2);
//       const __m128i tempa2 = _mm_xor_si128(tempa1, temp2);
//
//       const __m128i tempb3 = _mm_load_si128(prandex);
//       _mm_store_si128(prandex, tempa2);
//       _mm_store_si128(prand, tempb3);
//       break;
//     }
//     case 0x1c: {
//       const __m128i temp1 = _mm_load_si128(pbuf);
//       const __m128i temp2 = _mm_load_si128(prandex);
//       const __m128i add1 = _mm_xor_si128(temp1, temp2);
//       const __m128i clprod1 = _mm_clmulepi64_si128(add1, add1, 0x10);
//       acc = _mm_xor_si128(clprod1, acc);
//
//       const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp2);
//       const __m128i tempa2 = _mm_xor_si128(tempa1, temp2);
//
//       const __m128i tempa3 = _mm_load_si128(prand);
//       _mm_store_si128(prand, tempa2);
//
//       acc = _mm_xor_si128(tempa3, acc);
//
//       const __m128i tempb1 = _mm_mulhrs_epi16(acc, tempa3);
//       const __m128i tempb2 = _mm_xor_si128(tempb1, tempa3);
//       _mm_store_si128(prandex, tempb2);
//       break;
//     }
//     }
//   }
//   return acc;
// }

// hashes 64 bytes only by doing a carryless multiplication and reduction of the
// repeated 64 byte sequence 16 times, returning a 64 bit hash value
// uint64_t verusclhash(void *random, const unsigned char buf[64],
//                      uint64_t keyMask, __m128i **pMoveScratch) {
//   __m128i acc = __verusclmulwithoutreduction64alignedrepeat(
//       (__m128i *)random, (const __m128i *)buf, keyMask, pMoveScratch);
//   acc = _mm_xor_si128(acc, lazyLengthHash(1024, 64));
//   return precompReduction64(acc);
// }

__m128i __verusclmulwithoutreduction64alignedrepeat_sv2_1(
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
  __m128i acc = _mm_load_si128(randomsource + (keyMask + 2));

  for (int64_t i = 0; i < 32; i++) {
    const uint64_t selector = _mm_cvtsi128_si64(acc);

    // get two random locations in the key, which will be mutated and swapped
    __m128i *const prand = randomsource + ((selector >> 5) & keyMask);
    __m128i *const prandex = randomsource + ((selector >> 32) & keyMask);

    *(pMoveScratch++) = prand;
    *(pMoveScratch++) = prandex;

    // select random start and order of pbuf processing
    pbuf = pbuf_copy + (selector & 3);

    switch (selector & 0x1c) {
    case 0: {
      const __m128i temp1 = _mm_load_si128(prandex);
      const __m128i temp2 = _mm_load_si128(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add1 = _mm_xor_si128(temp1, temp2);
      const __m128i clprod1 = _mm_clmulepi64_si128(add1, add1, 0x10);
      acc = _mm_xor_si128(clprod1, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128(tempa1, temp1);

      const __m128i temp12 = _mm_load_si128(prand);
      _mm_store_si128(prand, tempa2);

      const __m128i temp22 = _mm_load_si128(pbuf);
      const __m128i add12 = _mm_xor_si128(temp12, temp22);
      const __m128i clprod12 = _mm_clmulepi64_si128(add12, add12, 0x10);
      acc = _mm_xor_si128(clprod12, acc);

      const __m128i tempb1 = _mm_mulhrs_epi16(acc, temp12);
      const __m128i tempb2 = _mm_xor_si128(tempb1, temp12);
      _mm_store_si128(prandex, tempb2);
      break;
    }
    case 4: {
      const __m128i temp1 = _mm_load_si128(prand);
      const __m128i temp2 = _mm_load_si128(pbuf);
      const __m128i add1 = _mm_xor_si128(temp1, temp2);
      const __m128i clprod1 = _mm_clmulepi64_si128(add1, add1, 0x10);
      acc = _mm_xor_si128(clprod1, acc);
      const __m128i clprod2 = _mm_clmulepi64_si128(temp2, temp2, 0x10);
      acc = _mm_xor_si128(clprod2, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128(tempa1, temp1);

      const __m128i temp12 = _mm_load_si128(prandex);
      _mm_store_si128(prandex, tempa2);

      const __m128i temp22 = _mm_load_si128(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add12 = _mm_xor_si128(temp12, temp22);
      acc = _mm_xor_si128(add12, acc);

      const __m128i tempb1 = _mm_mulhrs_epi16(acc, temp12);
      const __m128i tempb2 = _mm_xor_si128(tempb1, temp12);
      _mm_store_si128(prand, tempb2);
      break;
    }
    case 8: {
      const __m128i temp1 = _mm_load_si128(prandex);
      const __m128i temp2 = _mm_load_si128(pbuf);
      const __m128i add1 = _mm_xor_si128(temp1, temp2);
      acc = _mm_xor_si128(add1, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128(tempa1, temp1);

      const __m128i temp12 = _mm_load_si128(prand);
      _mm_store_si128(prand, tempa2);

      const __m128i temp22 = _mm_load_si128(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add12 = _mm_xor_si128(temp12, temp22);
      const __m128i clprod12 = _mm_clmulepi64_si128(add12, add12, 0x10);
      acc = _mm_xor_si128(clprod12, acc);
      const __m128i clprod22 = _mm_clmulepi64_si128(temp22, temp22, 0x10);
      acc = _mm_xor_si128(clprod22, acc);

      const __m128i tempb1 = _mm_mulhrs_epi16(acc, temp12);
      const __m128i tempb2 = _mm_xor_si128(tempb1, temp12);
      _mm_store_si128(prandex, tempb2);
      break;
    }
    case 0xc: {
      const __m128i temp1 = _mm_load_si128(prand);
      const __m128i temp2 = _mm_load_si128(pbuf - (((selector & 1) << 1) - 1));
      const __m128i add1 = _mm_xor_si128(temp1, temp2);

      // cannot be zero here
      const int32_t divisor = (uint32_t)selector;

      acc = _mm_xor_si128(add1, acc);

      const int64_t dividend = _mm_cvtsi128_si64(acc);
      const __m128i modulo = _mm_cvtsi32_si128(dividend % divisor);
      acc = _mm_xor_si128(modulo, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp1);
      const __m128i tempa2 = _mm_xor_si128(tempa1, temp1);

      if (dividend & 1) {
        const __m128i temp12 = _mm_load_si128(prandex);
        _mm_store_si128(prandex, tempa2);

        const __m128i temp22 = _mm_load_si128(pbuf);
        const __m128i add12 = _mm_xor_si128(temp12, temp22);
        const __m128i clprod12 = _mm_clmulepi64_si128(add12, add12, 0x10);
        acc = _mm_xor_si128(clprod12, acc);
        const __m128i clprod22 = _mm_clmulepi64_si128(temp22, temp22, 0x10);
        acc = _mm_xor_si128(clprod22, acc);

        const __m128i tempb1 = _mm_mulhrs_epi16(acc, temp12);
        const __m128i tempb2 = _mm_xor_si128(tempb1, temp12);
        _mm_store_si128(prand, tempb2);
      } else {
        const __m128i tempb3 = _mm_load_si128(prandex);
        _mm_store_si128(prandex, tempa2);
        _mm_store_si128(prand, tempb3);
      }
      break;
    }
    case 0x10: {
      // a few AES operations
      const __m128i *rc = prand;
      __m128i tmp;

      __m128i temp1 = _mm_load_si128(pbuf - (((selector & 1) << 1) - 1));
      __m128i temp2 = _mm_load_si128(pbuf);

      AES2(temp1, temp2, 0);
      MIX2(temp1, temp2);

      AES2(temp1, temp2, 4);
      MIX2(temp1, temp2);

      AES2(temp1, temp2, 8);
      MIX2(temp1, temp2);

      acc = _mm_xor_si128(temp2, _mm_xor_si128(temp1, acc));

      const __m128i tempa1 = _mm_load_si128(prand);
      const __m128i tempa2 = _mm_mulhrs_epi16(acc, tempa1);
      const __m128i tempa3 = _mm_xor_si128(tempa1, tempa2);

      const __m128i tempa4 = _mm_load_si128(prandex);
      _mm_store_si128(prandex, tempa3);
      _mm_store_si128(prand, tempa4);
      break;
    }
    case 0x14: {
      // we'll just call this one the monkins loop, inspired by Chris - modified
      // to cast to uint64_t on shift for more variability in the loop
      const __m128i *buftmp = pbuf - (((selector & 1) << 1) - 1);
      __m128i tmp; // used by MIX2

      uint64_t rounds = selector >> 61; // loop randomly between 1 and 8 times
      __m128i *rc = prand;
      uint64_t aesroundoffset = 0;
      __m128i onekey;

      do {
        if (selector & (((uint64_t)0x10000000) << rounds)) {
          onekey = _mm_load_si128(rc++);
          const __m128i temp2 = _mm_load_si128(rounds & 1 ? pbuf : buftmp);
          const __m128i add1 = _mm_xor_si128(onekey, temp2);
          const __m128i clprod1 = _mm_clmulepi64_si128(add1, add1, 0x10);
          acc = _mm_xor_si128(clprod1, acc);
        } else {
          onekey = _mm_load_si128(rc++);
          __m128i temp2 = _mm_load_si128(rounds & 1 ? buftmp : pbuf);
          AES2(onekey, temp2, aesroundoffset);
          aesroundoffset += 4;
          MIX2(onekey, temp2);
          acc = _mm_xor_si128(onekey, acc);
          acc = _mm_xor_si128(temp2, acc);
        }
      } while (rounds--);

      const __m128i tempa1 = _mm_load_si128(prand);
      const __m128i tempa2 = _mm_mulhrs_epi16(acc, tempa1);
      const __m128i tempa3 = _mm_xor_si128(tempa1, tempa2);

      const __m128i tempa4 = _mm_load_si128(prandex);
      _mm_store_si128(prandex, tempa3);
      _mm_store_si128(prand, tempa4);
      break;
    }
    case 0x18: {
      const __m128i *buftmp = pbuf - (((selector & 1) << 1) - 1);

      uint64_t rounds = selector >> 61; // loop randomly between 1 and 8 times
      __m128i *rc = prand;
      __m128i onekey;

      do {
        if (selector & (((uint64_t)0x10000000) << rounds)) {
          onekey = _mm_load_si128(rc++);
          const __m128i temp2 = _mm_load_si128(rounds & 1 ? pbuf : buftmp);
          const __m128i add1 = _mm_xor_si128(onekey, temp2);
          // cannot be zero here, may be negative
          const int32_t divisor = (uint32_t)selector;
          const int64_t dividend = _mm_cvtsi128_si64(add1);
          const __m128i modulo = _mm_cvtsi32_si128(dividend % divisor);
          acc = _mm_xor_si128(modulo, acc);
        } else {
          onekey = _mm_load_si128(rc++);
          __m128i temp2 = _mm_load_si128(rounds & 1 ? buftmp : pbuf);
          const __m128i add1 = _mm_xor_si128(onekey, temp2);
          const __m128i clprod1 = _mm_clmulepi64_si128(add1, add1, 0x10);
          const __m128i clprod2 = _mm_mulhrs_epi16(acc, clprod1);
          acc = _mm_xor_si128(clprod2, acc);
        }
      } while (rounds--);

      const __m128i tempa3 = _mm_load_si128(prandex);
      const __m128i tempa4 = _mm_xor_si128(tempa3, acc);
      _mm_store_si128(prandex, tempa4);
      _mm_store_si128(prand, onekey);
      break;
    }
    case 0x1c: {
      const __m128i temp1 = _mm_load_si128(pbuf);
      const __m128i temp2 = _mm_load_si128(prandex);
      const __m128i add1 = _mm_xor_si128(temp1, temp2);
      const __m128i clprod1 = _mm_clmulepi64_si128(add1, add1, 0x10);
      acc = _mm_xor_si128(clprod1, acc);

      const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp2);
      const __m128i tempa2 = _mm_xor_si128(tempa1, temp2);

      const __m128i tempa3 = _mm_load_si128(prand);
      _mm_store_si128(prand, tempa2);

      acc = _mm_xor_si128(tempa3, acc);

      const __m128i tempb1 = _mm_mulhrs_epi16(acc, tempa3);
      const __m128i tempb2 = _mm_xor_si128(tempb1, tempa3);
      _mm_store_si128(prandex, tempb2);
      break;
    }
    }
  }
  return acc;
}

// __m128i __verusclmulwithoutreduction64alignedrepeat_sv2_2(
//     __m128i *randomsource, const __m128i buf[4], uint64_t keyMask,
//     __m128i **pMoveScratch) {
//   const __m128i pbuf_copy[4] = {_mm_xor_si128(buf[0], buf[2]),
//                                 _mm_xor_si128(buf[1], buf[3]), buf[2],
//                                 buf[3]};
//   const __m128i *pbuf;
//
//   // divide key mask by 16 from bytes to __m128i
//   keyMask >>= 4;
//
//   // the random buffer must have at least 32 16 byte dwords after the keymask
//   to
//   // work with this algorithm. we take the value from the last element inside
//   // the keyMask + 2, as that will never be used to xor into the accumulator
//   // before it is hashed with other values first
//   __m128i acc = _mm_load_si128(randomsource + (keyMask + 2));
//
//   for (int64_t i = 0; i < 32; i++) {
//     const uint64_t selector = _mm_cvtsi128_si64(acc);
//
//     // get two random locations in the key, which will be mutated and swapped
//     __m128i *prand = randomsource + ((selector >> 5) & keyMask);
//     __m128i *prandex = randomsource + ((selector >> 32) & keyMask);
//
//     *(pMoveScratch++) = prand;
//     *(pMoveScratch++) = prandex;
//
//     // select random start and order of pbuf processing
//     pbuf = pbuf_copy + (selector & 3);
//
//     switch (selector & 0x1c) {
//     case 0: {
//       const __m128i temp1 = _mm_load_si128(prandex);
//       const __m128i temp2 = _mm_load_si128(pbuf - (((selector & 1) << 1) -
//       1)); const __m128i add1 = _mm_xor_si128(temp1, temp2); const __m128i
//       clprod1 = _mm_clmulepi64_si128(add1, add1, 0x10); acc =
//       _mm_xor_si128(clprod1, acc);
//
//       const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp1);
//       const __m128i tempa2 = _mm_xor_si128(tempa1, temp1);
//
//       const __m128i temp12 = _mm_load_si128(prand);
//       _mm_store_si128(prand, tempa2);
//
//       const __m128i temp22 = _mm_load_si128(pbuf);
//       const __m128i add12 = _mm_xor_si128(temp12, temp22);
//       const __m128i clprod12 = _mm_clmulepi64_si128(add12, add12, 0x10);
//       acc = _mm_xor_si128(clprod12, acc);
//
//       const __m128i tempb1 = _mm_mulhrs_epi16(acc, temp12);
//       const __m128i tempb2 = _mm_xor_si128(tempb1, temp12);
//       _mm_store_si128(prandex, tempb2);
//       break;
//     }
//     case 4: {
//       const __m128i temp1 = _mm_load_si128(prand);
//       const __m128i temp2 = _mm_load_si128(pbuf);
//       const __m128i add1 = _mm_xor_si128(temp1, temp2);
//       const __m128i clprod1 = _mm_clmulepi64_si128(add1, add1, 0x10);
//       acc = _mm_xor_si128(clprod1, acc);
//       const __m128i clprod2 = _mm_clmulepi64_si128(temp2, temp2, 0x10);
//       acc = _mm_xor_si128(clprod2, acc);
//
//       const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp1);
//       const __m128i tempa2 = _mm_xor_si128(tempa1, temp1);
//
//       const __m128i temp12 = _mm_load_si128(prandex);
//       _mm_store_si128(prandex, tempa2);
//
//       const __m128i temp22 = _mm_load_si128(pbuf - (((selector & 1) << 1) -
//       1)); const __m128i add12 = _mm_xor_si128(temp12, temp22); acc =
//       _mm_xor_si128(add12, acc);
//
//       const __m128i tempb1 = _mm_mulhrs_epi16(acc, temp12);
//       const __m128i tempb2 = _mm_xor_si128(tempb1, temp12);
//       _mm_store_si128(prand, tempb2);
//       break;
//     }
//     case 8: {
//       const __m128i temp1 = _mm_load_si128(prandex);
//       const __m128i temp2 = _mm_load_si128(pbuf);
//       const __m128i add1 = _mm_xor_si128(temp1, temp2);
//       acc = _mm_xor_si128(add1, acc);
//
//       const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp1);
//       const __m128i tempa2 = _mm_xor_si128(tempa1, temp1);
//
//       const __m128i temp12 = _mm_load_si128(prand);
//       _mm_store_si128(prand, tempa2);
//
//       const __m128i temp22 = _mm_load_si128(pbuf - (((selector & 1) << 1) -
//       1)); const __m128i add12 = _mm_xor_si128(temp12, temp22); const __m128i
//       clprod12 = _mm_clmulepi64_si128(add12, add12, 0x10); acc =
//       _mm_xor_si128(clprod12, acc); const __m128i clprod22 =
//       _mm_clmulepi64_si128(temp22, temp22, 0x10); acc =
//       _mm_xor_si128(clprod22, acc);
//
//       const __m128i tempb1 = _mm_mulhrs_epi16(acc, temp12);
//       const __m128i tempb2 = _mm_xor_si128(tempb1, temp12);
//       _mm_store_si128(prandex, tempb2);
//       break;
//     }
//     case 0xc: {
//       const __m128i temp1 = _mm_load_si128(prand);
//       const __m128i temp2 = _mm_load_si128(pbuf - (((selector & 1) << 1) -
//       1)); const __m128i add1 = _mm_xor_si128(temp1, temp2);
//
//       // cannot be zero here
//       const int32_t divisor = (uint32_t)selector;
//
//       acc = _mm_xor_si128(add1, acc);
//
//       const int64_t dividend = _mm_cvtsi128_si64(acc);
//       const __m128i modulo = _mm_cvtsi32_si128(dividend % divisor);
//       acc = _mm_xor_si128(modulo, acc);
//
//       const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp1);
//       const __m128i tempa2 = _mm_xor_si128(tempa1, temp1);
//
//       if (dividend & 1) {
//         const __m128i temp12 = _mm_load_si128(prandex);
//         _mm_store_si128(prandex, tempa2);
//
//         const __m128i temp22 = _mm_load_si128(pbuf);
//         const __m128i add12 = _mm_xor_si128(temp12, temp22);
//         const __m128i clprod12 = _mm_clmulepi64_si128(add12, add12, 0x10);
//         acc = _mm_xor_si128(clprod12, acc);
//         const __m128i clprod22 = _mm_clmulepi64_si128(temp22, temp22, 0x10);
//         acc = _mm_xor_si128(clprod22, acc);
//
//         const __m128i tempb1 = _mm_mulhrs_epi16(acc, temp12);
//         const __m128i tempb2 = _mm_xor_si128(tempb1, temp12);
//         _mm_store_si128(prand, tempb2);
//       } else {
//         const __m128i tempb3 = _mm_load_si128(prandex);
//         _mm_store_si128(prandex, tempa2);
//         _mm_store_si128(prand, tempb3);
//         const __m128i tempb4 = _mm_load_si128(pbuf);
//         acc = _mm_xor_si128(tempb4, acc);
//       }
//       break;
//     }
//     case 0x10: {
//       // a few AES operations
//       __m128i tmp;
//
//       __m128i temp1 = _mm_load_si128(pbuf - (((selector & 1) << 1) - 1));
//       __m128i temp2 = _mm_load_si128(pbuf);
//
//       AES2(temp1, temp2, 0);
//       MIX2(temp1, temp2);
//
//       AES2(temp1, temp2, 4);
//       MIX2(temp1, temp2);
//
//       AES2(temp1, temp2, 8);
//       MIX2(temp1, temp2);
//
//       acc = _mm_xor_si128(temp2, _mm_xor_si128(temp1, acc));
//
//       const __m128i tempa1 = _mm_load_si128(prand);
//       const __m128i tempa2 = _mm_mulhrs_epi16(acc, tempa1);
//       const __m128i tempa3 = _mm_xor_si128(tempa1, tempa2);
//
//       const __m128i tempa4 = _mm_load_si128(prandex);
//       _mm_store_si128(prandex, tempa3);
//       _mm_store_si128(prand, tempa4);
//       break;
//     }
//     case 0x14: {
//       // we'll just call this one the monkins loop, inspired by Chris -
//       modified
//       // to cast to uint64_t on shift for more variability in the loop
//       const __m128i *buftmp = pbuf - (((selector & 1) << 1) - 1);
//       __m128i tmp; // used by MIX2
//
//       uint64_t rounds = selector >> 61; // loop randomly between 1 and 8
//       times
//       __m128i *rc = prand;
//       uint64_t aesroundoffset = 0;
//       __m128i onekey;
//
//       do {
//         if (selector & (((uint64_t)0x10000000) << rounds)) {
//           onekey = _mm_load_si128(rc++);
//           const __m128i temp2 = _mm_load_si128(rounds & 1 ? pbuf : buftmp);
//           const __m128i add1 = _mm_xor_si128(onekey, temp2);
//           const __m128i clprod1 = _mm_clmulepi64_si128(add1, add1, 0x10);
//           acc = _mm_xor_si128(clprod1, acc);
//         } else {
//           onekey = _mm_load_si128(rc++);
//           __m128i temp2 = _mm_load_si128(rounds & 1 ? buftmp : pbuf);
//           AES2(onekey, temp2, aesroundoffset);
//           aesroundoffset += 4;
//           MIX2(onekey, temp2);
//           acc = _mm_xor_si128(onekey, acc);
//           acc = _mm_xor_si128(temp2, acc);
//         }
//       } while (rounds--);
//
//       const __m128i tempa1 = _mm_load_si128(prand);
//       const __m128i tempa2 = _mm_mulhrs_epi16(acc, tempa1);
//       const __m128i tempa3 = _mm_xor_si128(tempa1, tempa2);
//
//       const __m128i tempa4 = _mm_load_si128(prandex);
//       _mm_store_si128(prandex, tempa3);
//       _mm_store_si128(prand, tempa4);
//       break;
//     }
//     case 0x18: {
//       const __m128i *buftmp = pbuf - (((selector & 1) << 1) - 1);
//
//       uint64_t rounds = selector >> 61; // loop randomly between 1 and 8
//       times
//       __m128i *rc = prand;
//       __m128i onekey;
//
//       do {
//         if (selector & (((uint64_t)0x10000000) << rounds)) {
//           onekey = _mm_load_si128(rc++);
//           const __m128i temp2 = _mm_load_si128(rounds & 1 ? pbuf : buftmp);
//           onekey = _mm_xor_si128(onekey, temp2);
//           // cannot be zero here, may be negative
//           const int32_t divisor = (uint32_t)selector;
//           const int64_t dividend = _mm_cvtsi128_si64(onekey);
//           const __m128i modulo = _mm_cvtsi32_si128(dividend % divisor);
//           acc = _mm_xor_si128(modulo, acc);
//         } else {
//           onekey = _mm_load_si128(rc++);
//           __m128i temp2 = _mm_load_si128(rounds & 1 ? buftmp : pbuf);
//           const __m128i add1 = _mm_xor_si128(onekey, temp2);
//           onekey = _mm_clmulepi64_si128(add1, add1, 0x10);
//           const __m128i clprod2 = _mm_mulhrs_epi16(acc, onekey);
//           acc = _mm_xor_si128(clprod2, acc);
//         }
//       } while (rounds--);
//
//       const __m128i tempa3 = _mm_load_si128(prandex);
//       const __m128i tempa4 = _mm_xor_si128(tempa3, acc);
//
//       _mm_store_si128(prandex, onekey);
//       _mm_store_si128(prand, tempa4);
//       break;
//     }
//     case 0x1c: {
//       const __m128i temp1 = _mm_load_si128(pbuf);
//       const __m128i temp2 = _mm_load_si128(prandex);
//       const __m128i add1 = _mm_xor_si128(temp1, temp2);
//       const __m128i clprod1 = _mm_clmulepi64_si128(add1, add1, 0x10);
//       acc = _mm_xor_si128(clprod1, acc);
//
//       const __m128i tempa1 = _mm_mulhrs_epi16(acc, temp2);
//       const __m128i tempa2 = _mm_xor_si128(tempa1, temp2);
//
//       const __m128i tempa3 = _mm_load_si128(prand);
//       _mm_store_si128(prand, tempa2);
//
//       acc = _mm_xor_si128(tempa3, acc);
//       const __m128i temp4 = _mm_load_si128(pbuf - (((selector & 1) << 1) -
//       1)); acc = _mm_xor_si128(temp4, acc); const __m128i tempb1 =
//       _mm_mulhrs_epi16(acc, tempa3); const __m128i tempb2 =
//       _mm_xor_si128(tempb1, tempa3); _mm_store_si128(prandex, tempb2); break;
//     }
//     }
//   }
//   return acc;
// }

// uint64_t verusclhash_sv2_2(void *random, const unsigned char buf[64],
//                            uint64_t keyMask, __m128i **pMoveScratch) {
//   __m128i acc = __verusclmulwithoutreduction64alignedrepeat_sv2_2(
//       (__m128i *)random, (const __m128i *)buf, keyMask, pMoveScratch);
//   acc = _mm_xor_si128(acc, lazyLengthHash(1024, 64));
//   return precompReduction64(acc);
// }

} // namespace

// hashes 64 bytes only by doing a carryless multiplication and reduction of the
// repeated 64 byte sequence 16 times, returning a 64 bit hash value
uint64_t verusclhash_sv2_1(void *random, const unsigned char buf[64],
                           uint64_t keyMask, __m128i **pMoveScratch) {
  __m128i acc = __verusclmulwithoutreduction64alignedrepeat_sv2_1(
      (__m128i *)random, (const __m128i *)buf, keyMask, pMoveScratch);
  acc = _mm_xor_si128(acc, lazyLengthHash(1024, 64));
  return precompReduction64(acc);
}
namespace Verus {
namespace {
inline __attribute__((always_inline)) void fixupkey(__m128i **pMoveScratch) {
  __m128i **ppfixup = pMoveScratch;
  constexpr uint64_t fixupofs = (keySizeInBytes >> 4);
  for (__m128i *pfixup = *ppfixup; pfixup; pfixup = *++ppfixup) {
    *pfixup = *(pfixup + fixupofs);
  }
}
} // namespace

MinerOpt::MinerOpt(std::span<uint8_t, 80> argnew, HashView target,
                   uint32_t seedOffset)
    : target(target), seedOffset(seedOffset) {
  set_header(argnew);
}
void MinerOpt::set_header(std::span<uint8_t, 80> header) {
  std::copy(header.begin(), header.end(), arg.begin());
  vh.reset();
  vh.write(arg.data(), arg.size(), haraka512);

  u128 *hashKey = (u128 *)key;
  uint8_t *hasherrefresh = ((uint8_t *)hashKey) + keySizeInBytes;
  unsigned char *pkey = ((unsigned char *)hashKey);
  unsigned char *psrc = vh.curBuf;
  for (size_t i = 0; i < key256blocks; i++) {
    haraka256(pkey, psrc);
    psrc = pkey;
    pkey += 32;
  }
  memcpy(hasherrefresh, hashKey, keyRefreshsize);
  memset(hasherrefresh + keyRefreshsize, 0, keysize - keyRefreshsize);
}

auto MinerOpt::mine(uint32_t count) -> std::optional<Success> {

  alignas(32) Hash curHash;

  u128 *hashKey = (u128 *)key;
  uint8_t *const hasherrefresh = ((uint8_t *)hashKey) + keysize;
  __m128i **const pMoveScratch = (__m128i **)(hasherrefresh + keyRefreshsize);
  unsigned char *const curBuf = vh.curBuf;

  uint32_t &nonce = *reinterpret_cast<uint32_t *>(curBuf + 32 + (76 - 64));
  uint64_t i, end = seedOffset + count;
  for (i = seedOffset; i != end; i++) {
    nonce = i;

    // prepare the buffer
    *(u128 *)(curBuf + 32 + 16) = *(u128 *)(curBuf);

    // run verusclhash on the buffer
    __m128i acc = __verusclmulwithoutreduction64alignedrepeat_sv2_1(
        hashKey, (const __m128i *)curBuf, keyMask, pMoveScratch);
    acc = _mm_xor_si128(acc, lazyLengthHash(1024, 64));
    const uint64_t intermediate = precompReduction64(acc);

    *(uint64_t *)(curBuf + 32 + 16) = intermediate;
    *(uint64_t *)(curBuf + 32 + 16 + 8) = intermediate;

    haraka512_keyed_local(curHash.data(), curBuf,
                          hashKey + (intermediate & keyMask16));

    // refresh the key
    fixupkey(pMoveScratch);
    if (curHash[0] != 0 || curHash[1] != 0 ||
        curHash[2] > 16 /* curHash > curTarget */)
      continue;

    *reinterpret_cast<uint32_t *>(arg.data() + 76) = i;
    lastCount = i;
    return Success{curHash, arg};
  }
  lastCount = i;
  return {};
}
} // namespace Verus
