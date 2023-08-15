/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#include "inc_vendor.h"
#include "inc_types.h"
#include "inc_platform.h"

// bit rotates
//
// For HC_CPU_OPENCL_EMU_H we dont need to care about vector functions
// The VECT_SIZE is guaranteed to be set to 1 from cpu_opencl_emu.h

DECLSPEC u32x hc_rotl32 (const u32x a, const int n)
{
  #if   defined HC_CPU_OPENCL_EMU_H
  return rotl32 (a, n);
  #elif defined IS_CUDA || defined IS_HIP
  return rotl32 (a, n);
  #else
  #ifdef USE_ROTATE
  return rotate (a, make_u32x (n));
  #else
  return ((a << n) | (a >> (32 - n)));
  #endif
  #endif
}

DECLSPEC u32x hc_rotr32 (const u32x a, const int n)
{
  #if   defined HC_CPU_OPENCL_EMU_H
  return rotr32 (a, n);
  #elif defined IS_CUDA || defined IS_HIP
  return rotr32 (a, n);
  #else
  #ifdef USE_ROTATE
  return rotate (a, make_u32x (32 - n));
  #else
  return ((a >> n) | (a << (32 - n)));
  #endif
  #endif
}

DECLSPEC u32 hc_rotl32_S (const u32 a, const int n)
{
  #if   defined HC_CPU_OPENCL_EMU_H
  return rotl32 (a, n);
  #elif defined IS_CUDA || defined IS_HIP
  return rotl32_S (a, n);
  #else
  #ifdef USE_ROTATE
  return rotate (a, (u32) (n));
  #else
  return ((a << n) | (a >> (32 - n)));
  #endif
  #endif
}

DECLSPEC u32 hc_rotr32_S (const u32 a, const int n)
{
  #if   defined HC_CPU_OPENCL_EMU_H
  return rotr32 (a, n);
  #elif defined IS_CUDA || defined IS_HIP
  return rotr32_S (a, n);
  #else
  #ifdef USE_ROTATE
  return rotate (a, (u32) (32 - n));
  #else
  return ((a >> n) | (a << (32 - n)));
  #endif
  #endif
}

DECLSPEC u64x hc_rotl64 (const u64x a, const int n)
{
  #if   defined HC_CPU_OPENCL_EMU_H
  return rotl64 (a, n);
  #elif defined IS_CUDA
  return rotl64 (a, n);
  #elif (defined IS_AMD || defined IS_HIP)
  return rotl64 (a, n);
  #else
  #ifdef USE_ROTATE
  return rotate (a, make_u64x (n));
  #else
  return ((a << n) | (a >> (64 - n)));
  #endif
  #endif
}

DECLSPEC u64x hc_rotr64 (const u64x a, const int n)
{
  #if   defined HC_CPU_OPENCL_EMU_H
  return rotr64 (a, n);
  #elif defined IS_CUDA
  return rotr64 (a, n);
  #elif (defined IS_AMD || defined IS_HIP)
  return rotr64 (a, n);
  #else
  #ifdef USE_ROTATE
  return rotate (a, make_u64x (64 - n));
  #else
  return ((a >> n) | (a << (64 - n)));
  #endif
  #endif
}

DECLSPEC u64 hc_rotl64_S (const u64 a, const int n)
{
  #if   defined HC_CPU_OPENCL_EMU_H
  return rotl64 (a, n);
  #elif defined IS_CUDA
  return rotl64_S (a, n);
  #elif (defined IS_AMD || defined IS_HIP)
  return rotl64_S (a, n);
  #else
  #ifdef USE_ROTATE
  return rotate (a, (u64) (n));
  #else
  return ((a << n) | (a >> (64 - n)));
  #endif
  #endif
}


// bitwise swap

DECLSPEC u32x hc_swap32 (const u32x v)
{
  u32x r;

  #ifdef HC_CPU_OPENCL_EMU_H
  r = byte_swap_32 (v);
  #else
  #if   (defined IS_AMD || defined IS_HIP) && HAS_VPERM == 1

  const u32 m = 0x00010203;

  #if VECT_SIZE == 1
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r) : "v"(v), "v"(m));
  #endif

  #if VECT_SIZE >= 2
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.s0) : "v"(v.s0), "v"(m));
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.s1) : "v"(v.s1), "v"(m));
  #endif

  #if VECT_SIZE >= 4
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.s2) : "v"(v.s2), "v"(m));
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.s3) : "v"(v.s3), "v"(m));
  #endif

  #if VECT_SIZE >= 8
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.s4) : "v"(v.s4), "v"(m));
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.s5) : "v"(v.s5), "v"(m));
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.s6) : "v"(v.s6), "v"(m));
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.s7) : "v"(v.s7), "v"(m));
  #endif

  #if VECT_SIZE >= 16
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.s8) : "v"(v.s8), "v"(m));
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.s9) : "v"(v.s9), "v"(m));
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.sa) : "v"(v.sa), "v"(m));
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.sb) : "v"(v.sb), "v"(m));
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.sc) : "v"(v.sc), "v"(m));
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.sd) : "v"(v.sd), "v"(m));
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.se) : "v"(v.se), "v"(m));
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r.sf) : "v"(v.sf), "v"(m));
  #endif

  #elif defined IS_NV  && HAS_PRMT  == 1

  #if VECT_SIZE == 1
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r) : "r"(v));
  #endif

  #if VECT_SIZE >= 2
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.s0) : "r"(v.s0));
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.s1) : "r"(v.s1));
  #endif

  #if VECT_SIZE >= 4
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.s2) : "r"(v.s2));
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.s3) : "r"(v.s3));
  #endif

  #if VECT_SIZE >= 8
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.s4) : "r"(v.s4));
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.s5) : "r"(v.s5));
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.s6) : "r"(v.s6));
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.s7) : "r"(v.s7));
  #endif

  #if VECT_SIZE >= 16
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.s8) : "r"(v.s8));
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.s9) : "r"(v.s9));
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.sa) : "r"(v.sa));
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.sb) : "r"(v.sb));
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.sc) : "r"(v.sc));
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.sd) : "r"(v.sd));
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.se) : "r"(v.se));
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r.sf) : "r"(v.sf));
  #endif

  #else

  #if defined USE_BITSELECT && defined USE_ROTATE
  r = bitselect (rotate (v, make_u32x (24)),
                 rotate (v, make_u32x ( 8)),
                            make_u32x (0x00ff00ff));
  #else
  r = ((v & make_u32x (0xff000000)) >> 24)
    | ((v & make_u32x (0x00ff0000)) >>  8)
    | ((v & make_u32x (0x0000ff00)) <<  8)
    | ((v & make_u32x (0x000000ff)) << 24);
  #endif

  #endif

  #endif

  return r;
}

DECLSPEC u32 hc_swap32_S (const u32 v)
{
  u32 r;

  #ifdef HC_CPU_OPENCL_EMU_H
  r = byte_swap_32 (v);
  #else
  #if   (defined IS_AMD || defined IS_HIP) && HAS_VPERM == 1
  __asm__ __volatile__ ("V_PERM_B32 %0, 0, %1, %2;" : "=v"(r) : "v"(v), "v"(0x00010203));
  #elif defined IS_NV  && HAS_PRMT  == 1
  asm volatile ("prmt.b32 %0, %1, 0, 0x0123;" : "=r"(r) : "r"(v));
  #else
  #ifdef USE_SWIZZLE
  r = as_uint (as_uchar4 (v).s3210);
  #else
  r = ((v & 0xff000000) >> 24)
    | ((v & 0x00ff0000) >>  8)
    | ((v & 0x0000ff00) <<  8)
    | ((v & 0x000000ff) << 24);
  #endif
  #endif
  #endif

  return r;
}



#ifdef IS_GENERIC

DECLSPEC u32x hc_bytealign_be (const u32x a, const u32x b, const int c)
{
  u32x r = 0;

  const int cm = c & 3;

       if (cm == 0) { r = b;                     }
  else if (cm == 1) { r = (a << 24) | (b >>  8); }
  else if (cm == 2) { r = (a << 16) | (b >> 16); }
  else if (cm == 3) { r = (a <<  8) | (b >> 24); }

  return r;
}

DECLSPEC u32 hc_bytealign_be_S (const u32 a, const u32 b, const int c)
{
  u32 r = 0;

  const int cm = c & 3;

       if (cm == 0) { r = b;                     }
  else if (cm == 1) { r = (a << 24) | (b >>  8); }
  else if (cm == 2) { r = (a << 16) | (b >> 16); }
  else if (cm == 3) { r = (a <<  8) | (b >> 24); }

  return r;
}

DECLSPEC u32x hc_bytealign (const u32x a, const u32x b, const int c)
{
  u32x r = 0;

  const int cm = c & 3;

       if (cm == 0) { r = b;                     }
  else if (cm == 1) { r = (a >> 24) | (b <<  8); }
  else if (cm == 2) { r = (a >> 16) | (b << 16); }
  else if (cm == 3) { r = (a >>  8) | (b << 24); }

  return r;
}

DECLSPEC u32 hc_bytealign_S (const u32 a, const u32 b, const int c)
{
  u32 r = 0;

  const int cm = c & 3;

       if (cm == 0) { r = b;                     }
  else if (cm == 1) { r = (a >> 24) | (b <<  8); }
  else if (cm == 2) { r = (a >> 16) | (b << 16); }
  else if (cm == 3) { r = (a >>  8) | (b << 24); }

  return r;
}

DECLSPEC u32x hc_add3 (const u32x a, const u32x b, const u32x c)
{
  return a + b + c;
}

DECLSPEC u32 hc_add3_S (const u32 a, const u32 b, const u32 c)
{
  return a + b + c;
}


#endif


DECLSPEC void set_mark_1x4_S (PRIVATE_AS u32 *v, const u32 offset)
{
  const u32 c = (offset & 15) / 4;
  const u32 r = 0xff << ((offset & 3) * 8);

  v[0] = (c == 0) ? r : 0;
  v[1] = (c == 1) ? r : 0;
  v[2] = (c == 2) ? r : 0;
  v[3] = (c == 3) ? r : 0;
}

DECLSPEC void append_helper_1x4_S (PRIVATE_AS u32 *r, const u32 v, PRIVATE_AS const u32 *m)
{
  r[0] |= v & m[0];
  r[1] |= v & m[1];
  r[2] |= v & m[2];
  r[3] |= v & m[3];
}



DECLSPEC void append_0x80_4x4_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, PRIVATE_AS u32 *w2, PRIVATE_AS u32 *w3, const u32 offset)
{
  u32 v[4];

  set_mark_1x4_S (v, offset);

  const u32 offset16 = offset / 16;

  append_helper_1x4_S (w0, ((offset16 == 0) ? 0x80808080 : 0), v);
  append_helper_1x4_S (w1, ((offset16 == 1) ? 0x80808080 : 0), v);
  append_helper_1x4_S (w2, ((offset16 == 2) ? 0x80808080 : 0), v);
  append_helper_1x4_S (w3, ((offset16 == 3) ? 0x80808080 : 0), v);
}





DECLSPEC void switch_buffer_by_offset_carry_be_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, PRIVATE_AS u32 *w2, PRIVATE_AS u32 *w3, PRIVATE_AS u32 *c0, PRIVATE_AS u32 *c1, PRIVATE_AS u32 *c2, PRIVATE_AS u32 *c3, const u32 offset)
{
  const int offset_switch = offset / 4;

  #if ((defined IS_AMD || defined IS_HIP) && HAS_VPERM == 0) || defined IS_GENERIC
  switch (offset_switch)
  {
    case  0:
      c0[0] = hc_bytealign_be_S (w3[3],     0, offset);
      w3[3] = hc_bytealign_be_S (w3[2], w3[3], offset);
      w3[2] = hc_bytealign_be_S (w3[1], w3[2], offset);
      w3[1] = hc_bytealign_be_S (w3[0], w3[1], offset);
      w3[0] = hc_bytealign_be_S (w2[3], w3[0], offset);
      w2[3] = hc_bytealign_be_S (w2[2], w2[3], offset);
      w2[2] = hc_bytealign_be_S (w2[1], w2[2], offset);
      w2[1] = hc_bytealign_be_S (w2[0], w2[1], offset);
      w2[0] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w1[3] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w1[2] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w1[1] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w1[0] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w0[3] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w0[2] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w0[1] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w0[0] = hc_bytealign_be_S (    0, w0[0], offset);

      break;

    case  1:
      c0[1] = hc_bytealign_be_S (w3[3],     0, offset);
      c0[0] = hc_bytealign_be_S (w3[2], w3[3], offset);
      w3[3] = hc_bytealign_be_S (w3[1], w3[2], offset);
      w3[2] = hc_bytealign_be_S (w3[0], w3[1], offset);
      w3[1] = hc_bytealign_be_S (w2[3], w3[0], offset);
      w3[0] = hc_bytealign_be_S (w2[2], w2[3], offset);
      w2[3] = hc_bytealign_be_S (w2[1], w2[2], offset);
      w2[2] = hc_bytealign_be_S (w2[0], w2[1], offset);
      w2[1] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w2[0] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w1[3] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w1[2] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w1[1] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w1[0] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w0[3] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w0[2] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w0[1] = hc_bytealign_be_S (    0, w0[0], offset);
      w0[0] = 0;

      break;

    case  2:
      c0[2] = hc_bytealign_be_S (w3[3],     0, offset);
      c0[1] = hc_bytealign_be_S (w3[2], w3[3], offset);
      c0[0] = hc_bytealign_be_S (w3[1], w3[2], offset);
      w3[3] = hc_bytealign_be_S (w3[0], w3[1], offset);
      w3[2] = hc_bytealign_be_S (w2[3], w3[0], offset);
      w3[1] = hc_bytealign_be_S (w2[2], w2[3], offset);
      w3[0] = hc_bytealign_be_S (w2[1], w2[2], offset);
      w2[3] = hc_bytealign_be_S (w2[0], w2[1], offset);
      w2[2] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w2[1] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w2[0] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w1[3] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w1[2] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w1[1] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w1[0] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w0[3] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w0[2] = hc_bytealign_be_S (    0, w0[0], offset);
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  3:
      c0[3] = hc_bytealign_be_S (w3[3],     0, offset);
      c0[2] = hc_bytealign_be_S (w3[2], w3[3], offset);
      c0[1] = hc_bytealign_be_S (w3[1], w3[2], offset);
      c0[0] = hc_bytealign_be_S (w3[0], w3[1], offset);
      w3[3] = hc_bytealign_be_S (w2[3], w3[0], offset);
      w3[2] = hc_bytealign_be_S (w2[2], w2[3], offset);
      w3[1] = hc_bytealign_be_S (w2[1], w2[2], offset);
      w3[0] = hc_bytealign_be_S (w2[0], w2[1], offset);
      w2[3] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w2[2] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w2[1] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w2[0] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w1[3] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w1[2] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w1[1] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w1[0] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w0[3] = hc_bytealign_be_S (    0, w0[0], offset);
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  4:
      c1[0] = hc_bytealign_be_S (w3[3],     0, offset);
      c0[3] = hc_bytealign_be_S (w3[2], w3[3], offset);
      c0[2] = hc_bytealign_be_S (w3[1], w3[2], offset);
      c0[1] = hc_bytealign_be_S (w3[0], w3[1], offset);
      c0[0] = hc_bytealign_be_S (w2[3], w3[0], offset);
      w3[3] = hc_bytealign_be_S (w2[2], w2[3], offset);
      w3[2] = hc_bytealign_be_S (w2[1], w2[2], offset);
      w3[1] = hc_bytealign_be_S (w2[0], w2[1], offset);
      w3[0] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w2[3] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w2[2] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w2[1] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w2[0] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w1[3] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w1[2] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w1[1] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w1[0] = hc_bytealign_be_S (    0, w0[0], offset);
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  5:
      c1[1] = hc_bytealign_be_S (w3[3],     0, offset);
      c1[0] = hc_bytealign_be_S (w3[2], w3[3], offset);
      c0[3] = hc_bytealign_be_S (w3[1], w3[2], offset);
      c0[2] = hc_bytealign_be_S (w3[0], w3[1], offset);
      c0[1] = hc_bytealign_be_S (w2[3], w3[0], offset);
      c0[0] = hc_bytealign_be_S (w2[2], w2[3], offset);
      w3[3] = hc_bytealign_be_S (w2[1], w2[2], offset);
      w3[2] = hc_bytealign_be_S (w2[0], w2[1], offset);
      w3[1] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w3[0] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w2[3] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w2[2] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w2[1] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w2[0] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w1[3] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w1[2] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w1[1] = hc_bytealign_be_S (    0, w0[0], offset);
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  6:
      c1[2] = hc_bytealign_be_S (w3[3],     0, offset);
      c1[1] = hc_bytealign_be_S (w3[2], w3[3], offset);
      c1[0] = hc_bytealign_be_S (w3[1], w3[2], offset);
      c0[3] = hc_bytealign_be_S (w3[0], w3[1], offset);
      c0[2] = hc_bytealign_be_S (w2[3], w3[0], offset);
      c0[1] = hc_bytealign_be_S (w2[2], w2[3], offset);
      c0[0] = hc_bytealign_be_S (w2[1], w2[2], offset);
      w3[3] = hc_bytealign_be_S (w2[0], w2[1], offset);
      w3[2] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w3[1] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w3[0] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w2[3] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w2[2] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w2[1] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w2[0] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w1[3] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w1[2] = hc_bytealign_be_S (    0, w0[0], offset);
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  7:
      c1[3] = hc_bytealign_be_S (w3[3],     0, offset);
      c1[2] = hc_bytealign_be_S (w3[2], w3[3], offset);
      c1[1] = hc_bytealign_be_S (w3[1], w3[2], offset);
      c1[0] = hc_bytealign_be_S (w3[0], w3[1], offset);
      c0[3] = hc_bytealign_be_S (w2[3], w3[0], offset);
      c0[2] = hc_bytealign_be_S (w2[2], w2[3], offset);
      c0[1] = hc_bytealign_be_S (w2[1], w2[2], offset);
      c0[0] = hc_bytealign_be_S (w2[0], w2[1], offset);
      w3[3] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w3[2] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w3[1] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w3[0] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w2[3] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w2[2] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w2[1] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w2[0] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w1[3] = hc_bytealign_be_S (    0, w0[0], offset);
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  8:
      c2[0] = hc_bytealign_be_S (w3[3],     0, offset);
      c1[3] = hc_bytealign_be_S (w3[2], w3[3], offset);
      c1[2] = hc_bytealign_be_S (w3[1], w3[2], offset);
      c1[1] = hc_bytealign_be_S (w3[0], w3[1], offset);
      c1[0] = hc_bytealign_be_S (w2[3], w3[0], offset);
      c0[3] = hc_bytealign_be_S (w2[2], w2[3], offset);
      c0[2] = hc_bytealign_be_S (w2[1], w2[2], offset);
      c0[1] = hc_bytealign_be_S (w2[0], w2[1], offset);
      c0[0] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w3[3] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w3[2] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w3[1] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w3[0] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w2[3] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w2[2] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w2[1] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w2[0] = hc_bytealign_be_S (    0, w0[0], offset);
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  9:
      c2[1] = hc_bytealign_be_S (w3[3],     0, offset);
      c2[0] = hc_bytealign_be_S (w3[2], w3[3], offset);
      c1[3] = hc_bytealign_be_S (w3[1], w3[2], offset);
      c1[2] = hc_bytealign_be_S (w3[0], w3[1], offset);
      c1[1] = hc_bytealign_be_S (w2[3], w3[0], offset);
      c1[0] = hc_bytealign_be_S (w2[2], w2[3], offset);
      c0[3] = hc_bytealign_be_S (w2[1], w2[2], offset);
      c0[2] = hc_bytealign_be_S (w2[0], w2[1], offset);
      c0[1] = hc_bytealign_be_S (w1[3], w2[0], offset);
      c0[0] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w3[3] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w3[2] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w3[1] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w3[0] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w2[3] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w2[2] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w2[1] = hc_bytealign_be_S (    0, w0[0], offset);
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 10:
      c2[2] = hc_bytealign_be_S (w3[3],     0, offset);
      c2[1] = hc_bytealign_be_S (w3[2], w3[3], offset);
      c2[0] = hc_bytealign_be_S (w3[1], w3[2], offset);
      c1[3] = hc_bytealign_be_S (w3[0], w3[1], offset);
      c1[2] = hc_bytealign_be_S (w2[3], w3[0], offset);
      c1[1] = hc_bytealign_be_S (w2[2], w2[3], offset);
      c1[0] = hc_bytealign_be_S (w2[1], w2[2], offset);
      c0[3] = hc_bytealign_be_S (w2[0], w2[1], offset);
      c0[2] = hc_bytealign_be_S (w1[3], w2[0], offset);
      c0[1] = hc_bytealign_be_S (w1[2], w1[3], offset);
      c0[0] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w3[3] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w3[2] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w3[1] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w3[0] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w2[3] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w2[2] = hc_bytealign_be_S (    0, w0[0], offset);
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 11:
      c2[3] = hc_bytealign_be_S (w3[3],     0, offset);
      c2[2] = hc_bytealign_be_S (w3[2], w3[3], offset);
      c2[1] = hc_bytealign_be_S (w3[1], w3[2], offset);
      c2[0] = hc_bytealign_be_S (w3[0], w3[1], offset);
      c1[3] = hc_bytealign_be_S (w2[3], w3[0], offset);
      c1[2] = hc_bytealign_be_S (w2[2], w2[3], offset);
      c1[1] = hc_bytealign_be_S (w2[1], w2[2], offset);
      c1[0] = hc_bytealign_be_S (w2[0], w2[1], offset);
      c0[3] = hc_bytealign_be_S (w1[3], w2[0], offset);
      c0[2] = hc_bytealign_be_S (w1[2], w1[3], offset);
      c0[1] = hc_bytealign_be_S (w1[1], w1[2], offset);
      c0[0] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w3[3] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w3[2] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w3[1] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w3[0] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w2[3] = hc_bytealign_be_S (    0, w0[0], offset);
      w2[2] = 0;
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 12:
      c3[0] = hc_bytealign_be_S (w3[3],     0, offset);
      c2[3] = hc_bytealign_be_S (w3[2], w3[3], offset);
      c2[2] = hc_bytealign_be_S (w3[1], w3[2], offset);
      c2[1] = hc_bytealign_be_S (w3[0], w3[1], offset);
      c2[0] = hc_bytealign_be_S (w2[3], w3[0], offset);
      c1[3] = hc_bytealign_be_S (w2[2], w2[3], offset);
      c1[2] = hc_bytealign_be_S (w2[1], w2[2], offset);
      c1[1] = hc_bytealign_be_S (w2[0], w2[1], offset);
      c1[0] = hc_bytealign_be_S (w1[3], w2[0], offset);
      c0[3] = hc_bytealign_be_S (w1[2], w1[3], offset);
      c0[2] = hc_bytealign_be_S (w1[1], w1[2], offset);
      c0[1] = hc_bytealign_be_S (w1[0], w1[1], offset);
      c0[0] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w3[3] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w3[2] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w3[1] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w3[0] = hc_bytealign_be_S (    0, w0[0], offset);
      w2[3] = 0;
      w2[2] = 0;
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 13:
      c3[1] = hc_bytealign_be_S (w3[3],     0, offset);
      c3[0] = hc_bytealign_be_S (w3[2], w3[3], offset);
      c2[3] = hc_bytealign_be_S (w3[1], w3[2], offset);
      c2[2] = hc_bytealign_be_S (w3[0], w3[1], offset);
      c2[1] = hc_bytealign_be_S (w2[3], w3[0], offset);
      c2[0] = hc_bytealign_be_S (w2[2], w2[3], offset);
      c1[3] = hc_bytealign_be_S (w2[1], w2[2], offset);
      c1[2] = hc_bytealign_be_S (w2[0], w2[1], offset);
      c1[1] = hc_bytealign_be_S (w1[3], w2[0], offset);
      c1[0] = hc_bytealign_be_S (w1[2], w1[3], offset);
      c0[3] = hc_bytealign_be_S (w1[1], w1[2], offset);
      c0[2] = hc_bytealign_be_S (w1[0], w1[1], offset);
      c0[1] = hc_bytealign_be_S (w0[3], w1[0], offset);
      c0[0] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w3[3] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w3[2] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w3[1] = hc_bytealign_be_S (    0, w0[0], offset);
      w3[0] = 0;
      w2[3] = 0;
      w2[2] = 0;
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 14:
      c3[2] = hc_bytealign_be_S (w3[3],     0, offset);
      c3[1] = hc_bytealign_be_S (w3[2], w3[3], offset);
      c3[0] = hc_bytealign_be_S (w3[1], w3[2], offset);
      c2[3] = hc_bytealign_be_S (w3[0], w3[1], offset);
      c2[2] = hc_bytealign_be_S (w2[3], w3[0], offset);
      c2[1] = hc_bytealign_be_S (w2[2], w2[3], offset);
      c2[0] = hc_bytealign_be_S (w2[1], w2[2], offset);
      c1[3] = hc_bytealign_be_S (w2[0], w2[1], offset);
      c1[2] = hc_bytealign_be_S (w1[3], w2[0], offset);
      c1[1] = hc_bytealign_be_S (w1[2], w1[3], offset);
      c1[0] = hc_bytealign_be_S (w1[1], w1[2], offset);
      c0[3] = hc_bytealign_be_S (w1[0], w1[1], offset);
      c0[2] = hc_bytealign_be_S (w0[3], w1[0], offset);
      c0[1] = hc_bytealign_be_S (w0[2], w0[3], offset);
      c0[0] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w3[3] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w3[2] = hc_bytealign_be_S (    0, w0[0], offset);
      w3[1] = 0;
      w3[0] = 0;
      w2[3] = 0;
      w2[2] = 0;
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 15:
      c3[3] = hc_bytealign_be_S (w3[3],     0, offset);
      c3[2] = hc_bytealign_be_S (w3[2], w3[3], offset);
      c3[1] = hc_bytealign_be_S (w3[1], w3[2], offset);
      c3[0] = hc_bytealign_be_S (w3[0], w3[1], offset);
      c2[3] = hc_bytealign_be_S (w2[3], w3[0], offset);
      c2[2] = hc_bytealign_be_S (w2[2], w2[3], offset);
      c2[1] = hc_bytealign_be_S (w2[1], w2[2], offset);
      c2[0] = hc_bytealign_be_S (w2[0], w2[1], offset);
      c1[3] = hc_bytealign_be_S (w1[3], w2[0], offset);
      c1[2] = hc_bytealign_be_S (w1[2], w1[3], offset);
      c1[1] = hc_bytealign_be_S (w1[1], w1[2], offset);
      c1[0] = hc_bytealign_be_S (w1[0], w1[1], offset);
      c0[3] = hc_bytealign_be_S (w0[3], w1[0], offset);
      c0[2] = hc_bytealign_be_S (w0[2], w0[3], offset);
      c0[1] = hc_bytealign_be_S (w0[1], w0[2], offset);
      c0[0] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w3[3] = hc_bytealign_be_S (    0, w0[0], offset);
      w3[2] = 0;
      w3[1] = 0;
      w3[0] = 0;
      w2[3] = 0;
      w2[2] = 0;
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;
  }
  #endif

  #if ((defined IS_AMD || defined IS_HIP) && HAS_VPERM == 1) || defined IS_NV

  #if defined IS_NV
  const int selector = (0x76543210 >> ((offset & 3) * 4)) & 0xffff;
  #endif

  #if (defined IS_AMD || defined IS_HIP)
  const int selector = l32_from_64_S (0x0706050403020100UL >> ((offset & 3) * 8));
  #endif

  switch (offset_switch)
  {
    case  0:
      c0[0] = hc_byte_perm_S (    0, w3[3], selector);
      w3[3] = hc_byte_perm_S (w3[3], w3[2], selector);
      w3[2] = hc_byte_perm_S (w3[2], w3[1], selector);
      w3[1] = hc_byte_perm_S (w3[1], w3[0], selector);
      w3[0] = hc_byte_perm_S (w3[0], w2[3], selector);
      w2[3] = hc_byte_perm_S (w2[3], w2[2], selector);
      w2[2] = hc_byte_perm_S (w2[2], w2[1], selector);
      w2[1] = hc_byte_perm_S (w2[1], w2[0], selector);
      w2[0] = hc_byte_perm_S (w2[0], w1[3], selector);
      w1[3] = hc_byte_perm_S (w1[3], w1[2], selector);
      w1[2] = hc_byte_perm_S (w1[2], w1[1], selector);
      w1[1] = hc_byte_perm_S (w1[1], w1[0], selector);
      w1[0] = hc_byte_perm_S (w1[0], w0[3], selector);
      w0[3] = hc_byte_perm_S (w0[3], w0[2], selector);
      w0[2] = hc_byte_perm_S (w0[2], w0[1], selector);
      w0[1] = hc_byte_perm_S (w0[1], w0[0], selector);
      w0[0] = hc_byte_perm_S (w0[0],     0, selector);

      break;

    case  1:
      c0[1] = hc_byte_perm_S (    0, w3[3], selector);
      c0[0] = hc_byte_perm_S (w3[3], w3[2], selector);
      w3[3] = hc_byte_perm_S (w3[2], w3[1], selector);
      w3[2] = hc_byte_perm_S (w3[1], w3[0], selector);
      w3[1] = hc_byte_perm_S (w3[0], w2[3], selector);
      w3[0] = hc_byte_perm_S (w2[3], w2[2], selector);
      w2[3] = hc_byte_perm_S (w2[2], w2[1], selector);
      w2[2] = hc_byte_perm_S (w2[1], w2[0], selector);
      w2[1] = hc_byte_perm_S (w2[0], w1[3], selector);
      w2[0] = hc_byte_perm_S (w1[3], w1[2], selector);
      w1[3] = hc_byte_perm_S (w1[2], w1[1], selector);
      w1[2] = hc_byte_perm_S (w1[1], w1[0], selector);
      w1[1] = hc_byte_perm_S (w1[0], w0[3], selector);
      w1[0] = hc_byte_perm_S (w0[3], w0[2], selector);
      w0[3] = hc_byte_perm_S (w0[2], w0[1], selector);
      w0[2] = hc_byte_perm_S (w0[1], w0[0], selector);
      w0[1] = hc_byte_perm_S (w0[0],     0, selector);
      w0[0] = 0;

      break;

    case  2:
      c0[2] = hc_byte_perm_S (    0, w3[3], selector);
      c0[1] = hc_byte_perm_S (w3[3], w3[2], selector);
      c0[0] = hc_byte_perm_S (w3[2], w3[1], selector);
      w3[3] = hc_byte_perm_S (w3[1], w3[0], selector);
      w3[2] = hc_byte_perm_S (w3[0], w2[3], selector);
      w3[1] = hc_byte_perm_S (w2[3], w2[2], selector);
      w3[0] = hc_byte_perm_S (w2[2], w2[1], selector);
      w2[3] = hc_byte_perm_S (w2[1], w2[0], selector);
      w2[2] = hc_byte_perm_S (w2[0], w1[3], selector);
      w2[1] = hc_byte_perm_S (w1[3], w1[2], selector);
      w2[0] = hc_byte_perm_S (w1[2], w1[1], selector);
      w1[3] = hc_byte_perm_S (w1[1], w1[0], selector);
      w1[2] = hc_byte_perm_S (w1[0], w0[3], selector);
      w1[1] = hc_byte_perm_S (w0[3], w0[2], selector);
      w1[0] = hc_byte_perm_S (w0[2], w0[1], selector);
      w0[3] = hc_byte_perm_S (w0[1], w0[0], selector);
      w0[2] = hc_byte_perm_S (w0[0],     0, selector);
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  3:
      c0[3] = hc_byte_perm_S (    0, w3[3], selector);
      c0[2] = hc_byte_perm_S (w3[3], w3[2], selector);
      c0[1] = hc_byte_perm_S (w3[2], w3[1], selector);
      c0[0] = hc_byte_perm_S (w3[1], w3[0], selector);
      w3[3] = hc_byte_perm_S (w3[0], w2[3], selector);
      w3[2] = hc_byte_perm_S (w2[3], w2[2], selector);
      w3[1] = hc_byte_perm_S (w2[2], w2[1], selector);
      w3[0] = hc_byte_perm_S (w2[1], w2[0], selector);
      w2[3] = hc_byte_perm_S (w2[0], w1[3], selector);
      w2[2] = hc_byte_perm_S (w1[3], w1[2], selector);
      w2[1] = hc_byte_perm_S (w1[2], w1[1], selector);
      w2[0] = hc_byte_perm_S (w1[1], w1[0], selector);
      w1[3] = hc_byte_perm_S (w1[0], w0[3], selector);
      w1[2] = hc_byte_perm_S (w0[3], w0[2], selector);
      w1[1] = hc_byte_perm_S (w0[2], w0[1], selector);
      w1[0] = hc_byte_perm_S (w0[1], w0[0], selector);
      w0[3] = hc_byte_perm_S (w0[0],     0, selector);
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  4:
      c1[0] = hc_byte_perm_S (    0, w3[3], selector);
      c0[3] = hc_byte_perm_S (w3[3], w3[2], selector);
      c0[2] = hc_byte_perm_S (w3[2], w3[1], selector);
      c0[1] = hc_byte_perm_S (w3[1], w3[0], selector);
      c0[0] = hc_byte_perm_S (w3[0], w2[3], selector);
      w3[3] = hc_byte_perm_S (w2[3], w2[2], selector);
      w3[2] = hc_byte_perm_S (w2[2], w2[1], selector);
      w3[1] = hc_byte_perm_S (w2[1], w2[0], selector);
      w3[0] = hc_byte_perm_S (w2[0], w1[3], selector);
      w2[3] = hc_byte_perm_S (w1[3], w1[2], selector);
      w2[2] = hc_byte_perm_S (w1[2], w1[1], selector);
      w2[1] = hc_byte_perm_S (w1[1], w1[0], selector);
      w2[0] = hc_byte_perm_S (w1[0], w0[3], selector);
      w1[3] = hc_byte_perm_S (w0[3], w0[2], selector);
      w1[2] = hc_byte_perm_S (w0[2], w0[1], selector);
      w1[1] = hc_byte_perm_S (w0[1], w0[0], selector);
      w1[0] = hc_byte_perm_S (w0[0],     0, selector);
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  5:
      c1[1] = hc_byte_perm_S (    0, w3[3], selector);
      c1[0] = hc_byte_perm_S (w3[3], w3[2], selector);
      c0[3] = hc_byte_perm_S (w3[2], w3[1], selector);
      c0[2] = hc_byte_perm_S (w3[1], w3[0], selector);
      c0[1] = hc_byte_perm_S (w3[0], w2[3], selector);
      c0[0] = hc_byte_perm_S (w2[3], w2[2], selector);
      w3[3] = hc_byte_perm_S (w2[2], w2[1], selector);
      w3[2] = hc_byte_perm_S (w2[1], w2[0], selector);
      w3[1] = hc_byte_perm_S (w2[0], w1[3], selector);
      w3[0] = hc_byte_perm_S (w1[3], w1[2], selector);
      w2[3] = hc_byte_perm_S (w1[2], w1[1], selector);
      w2[2] = hc_byte_perm_S (w1[1], w1[0], selector);
      w2[1] = hc_byte_perm_S (w1[0], w0[3], selector);
      w2[0] = hc_byte_perm_S (w0[3], w0[2], selector);
      w1[3] = hc_byte_perm_S (w0[2], w0[1], selector);
      w1[2] = hc_byte_perm_S (w0[1], w0[0], selector);
      w1[1] = hc_byte_perm_S (w0[0],     0, selector);
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  6:
      c1[2] = hc_byte_perm_S (    0, w3[3], selector);
      c1[1] = hc_byte_perm_S (w3[3], w3[2], selector);
      c1[0] = hc_byte_perm_S (w3[2], w3[1], selector);
      c0[3] = hc_byte_perm_S (w3[1], w3[0], selector);
      c0[2] = hc_byte_perm_S (w3[0], w2[3], selector);
      c0[1] = hc_byte_perm_S (w2[3], w2[2], selector);
      c0[0] = hc_byte_perm_S (w2[2], w2[1], selector);
      w3[3] = hc_byte_perm_S (w2[1], w2[0], selector);
      w3[2] = hc_byte_perm_S (w2[0], w1[3], selector);
      w3[1] = hc_byte_perm_S (w1[3], w1[2], selector);
      w3[0] = hc_byte_perm_S (w1[2], w1[1], selector);
      w2[3] = hc_byte_perm_S (w1[1], w1[0], selector);
      w2[2] = hc_byte_perm_S (w1[0], w0[3], selector);
      w2[1] = hc_byte_perm_S (w0[3], w0[2], selector);
      w2[0] = hc_byte_perm_S (w0[2], w0[1], selector);
      w1[3] = hc_byte_perm_S (w0[1], w0[0], selector);
      w1[2] = hc_byte_perm_S (w0[0],     0, selector);
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  7:
      c1[3] = hc_byte_perm_S (    0, w3[3], selector);
      c1[2] = hc_byte_perm_S (w3[3], w3[2], selector);
      c1[1] = hc_byte_perm_S (w3[2], w3[1], selector);
      c1[0] = hc_byte_perm_S (w3[1], w3[0], selector);
      c0[3] = hc_byte_perm_S (w3[0], w2[3], selector);
      c0[2] = hc_byte_perm_S (w2[3], w2[2], selector);
      c0[1] = hc_byte_perm_S (w2[2], w2[1], selector);
      c0[0] = hc_byte_perm_S (w2[1], w2[0], selector);
      w3[3] = hc_byte_perm_S (w2[0], w1[3], selector);
      w3[2] = hc_byte_perm_S (w1[3], w1[2], selector);
      w3[1] = hc_byte_perm_S (w1[2], w1[1], selector);
      w3[0] = hc_byte_perm_S (w1[1], w1[0], selector);
      w2[3] = hc_byte_perm_S (w1[0], w0[3], selector);
      w2[2] = hc_byte_perm_S (w0[3], w0[2], selector);
      w2[1] = hc_byte_perm_S (w0[2], w0[1], selector);
      w2[0] = hc_byte_perm_S (w0[1], w0[0], selector);
      w1[3] = hc_byte_perm_S (w0[0],     0, selector);
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  8:
      c2[0] = hc_byte_perm_S (    0, w3[3], selector);
      c1[3] = hc_byte_perm_S (w3[3], w3[2], selector);
      c1[2] = hc_byte_perm_S (w3[2], w3[1], selector);
      c1[1] = hc_byte_perm_S (w3[1], w3[0], selector);
      c1[0] = hc_byte_perm_S (w3[0], w2[3], selector);
      c0[3] = hc_byte_perm_S (w2[3], w2[2], selector);
      c0[2] = hc_byte_perm_S (w2[2], w2[1], selector);
      c0[1] = hc_byte_perm_S (w2[1], w2[0], selector);
      c0[0] = hc_byte_perm_S (w2[0], w1[3], selector);
      w3[3] = hc_byte_perm_S (w1[3], w1[2], selector);
      w3[2] = hc_byte_perm_S (w1[2], w1[1], selector);
      w3[1] = hc_byte_perm_S (w1[1], w1[0], selector);
      w3[0] = hc_byte_perm_S (w1[0], w0[3], selector);
      w2[3] = hc_byte_perm_S (w0[3], w0[2], selector);
      w2[2] = hc_byte_perm_S (w0[2], w0[1], selector);
      w2[1] = hc_byte_perm_S (w0[1], w0[0], selector);
      w2[0] = hc_byte_perm_S (w0[0],     0, selector);
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  9:
      c2[1] = hc_byte_perm_S (    0, w3[3], selector);
      c2[0] = hc_byte_perm_S (w3[3], w3[2], selector);
      c1[3] = hc_byte_perm_S (w3[2], w3[1], selector);
      c1[2] = hc_byte_perm_S (w3[1], w3[0], selector);
      c1[1] = hc_byte_perm_S (w3[0], w2[3], selector);
      c1[0] = hc_byte_perm_S (w2[3], w2[2], selector);
      c0[3] = hc_byte_perm_S (w2[2], w2[1], selector);
      c0[2] = hc_byte_perm_S (w2[1], w2[0], selector);
      c0[1] = hc_byte_perm_S (w2[0], w1[3], selector);
      c0[0] = hc_byte_perm_S (w1[3], w1[2], selector);
      w3[3] = hc_byte_perm_S (w1[2], w1[1], selector);
      w3[2] = hc_byte_perm_S (w1[1], w1[0], selector);
      w3[1] = hc_byte_perm_S (w1[0], w0[3], selector);
      w3[0] = hc_byte_perm_S (w0[3], w0[2], selector);
      w2[3] = hc_byte_perm_S (w0[2], w0[1], selector);
      w2[2] = hc_byte_perm_S (w0[1], w0[0], selector);
      w2[1] = hc_byte_perm_S (w0[0],     0, selector);
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 10:
      c2[2] = hc_byte_perm_S (    0, w3[3], selector);
      c2[1] = hc_byte_perm_S (w3[3], w3[2], selector);
      c2[0] = hc_byte_perm_S (w3[2], w3[1], selector);
      c1[3] = hc_byte_perm_S (w3[1], w3[0], selector);
      c1[2] = hc_byte_perm_S (w3[0], w2[3], selector);
      c1[1] = hc_byte_perm_S (w2[3], w2[2], selector);
      c1[0] = hc_byte_perm_S (w2[2], w2[1], selector);
      c0[3] = hc_byte_perm_S (w2[1], w2[0], selector);
      c0[2] = hc_byte_perm_S (w2[0], w1[3], selector);
      c0[1] = hc_byte_perm_S (w1[3], w1[2], selector);
      c0[0] = hc_byte_perm_S (w1[2], w1[1], selector);
      w3[3] = hc_byte_perm_S (w1[1], w1[0], selector);
      w3[2] = hc_byte_perm_S (w1[0], w0[3], selector);
      w3[1] = hc_byte_perm_S (w0[3], w0[2], selector);
      w3[0] = hc_byte_perm_S (w0[2], w0[1], selector);
      w2[3] = hc_byte_perm_S (w0[1], w0[0], selector);
      w2[2] = hc_byte_perm_S (w0[0],     0, selector);
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 11:
      c2[3] = hc_byte_perm_S (    0, w3[3], selector);
      c2[2] = hc_byte_perm_S (w3[3], w3[2], selector);
      c2[1] = hc_byte_perm_S (w3[2], w3[1], selector);
      c2[0] = hc_byte_perm_S (w3[1], w3[0], selector);
      c1[3] = hc_byte_perm_S (w3[0], w2[3], selector);
      c1[2] = hc_byte_perm_S (w2[3], w2[2], selector);
      c1[1] = hc_byte_perm_S (w2[2], w2[1], selector);
      c1[0] = hc_byte_perm_S (w2[1], w2[0], selector);
      c0[3] = hc_byte_perm_S (w2[0], w1[3], selector);
      c0[2] = hc_byte_perm_S (w1[3], w1[2], selector);
      c0[1] = hc_byte_perm_S (w1[2], w1[1], selector);
      c0[0] = hc_byte_perm_S (w1[1], w1[0], selector);
      w3[3] = hc_byte_perm_S (w1[0], w0[3], selector);
      w3[2] = hc_byte_perm_S (w0[3], w0[2], selector);
      w3[1] = hc_byte_perm_S (w0[2], w0[1], selector);
      w3[0] = hc_byte_perm_S (w0[1], w0[0], selector);
      w2[3] = hc_byte_perm_S (w0[0],     0, selector);
      w2[2] = 0;
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 12:
      c3[0] = hc_byte_perm_S (    0, w3[3], selector);
      c2[3] = hc_byte_perm_S (w3[3], w3[2], selector);
      c2[2] = hc_byte_perm_S (w3[2], w3[1], selector);
      c2[1] = hc_byte_perm_S (w3[1], w3[0], selector);
      c2[0] = hc_byte_perm_S (w3[0], w2[3], selector);
      c1[3] = hc_byte_perm_S (w2[3], w2[2], selector);
      c1[2] = hc_byte_perm_S (w2[2], w2[1], selector);
      c1[1] = hc_byte_perm_S (w2[1], w2[0], selector);
      c1[0] = hc_byte_perm_S (w2[0], w1[3], selector);
      c0[3] = hc_byte_perm_S (w1[3], w1[2], selector);
      c0[2] = hc_byte_perm_S (w1[2], w1[1], selector);
      c0[1] = hc_byte_perm_S (w1[1], w1[0], selector);
      c0[0] = hc_byte_perm_S (w1[0], w0[3], selector);
      w3[3] = hc_byte_perm_S (w0[3], w0[2], selector);
      w3[2] = hc_byte_perm_S (w0[2], w0[1], selector);
      w3[1] = hc_byte_perm_S (w0[1], w0[0], selector);
      w3[0] = hc_byte_perm_S (w0[0],     0, selector);
      w2[3] = 0;
      w2[2] = 0;
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 13:
      c3[1] = hc_byte_perm_S (    0, w3[3], selector);
      c3[0] = hc_byte_perm_S (w3[3], w3[2], selector);
      c2[3] = hc_byte_perm_S (w3[2], w3[1], selector);
      c2[2] = hc_byte_perm_S (w3[1], w3[0], selector);
      c2[1] = hc_byte_perm_S (w3[0], w2[3], selector);
      c2[0] = hc_byte_perm_S (w2[3], w2[2], selector);
      c1[3] = hc_byte_perm_S (w2[2], w2[1], selector);
      c1[2] = hc_byte_perm_S (w2[1], w2[0], selector);
      c1[1] = hc_byte_perm_S (w2[0], w1[3], selector);
      c1[0] = hc_byte_perm_S (w1[3], w1[2], selector);
      c0[3] = hc_byte_perm_S (w1[2], w1[1], selector);
      c0[2] = hc_byte_perm_S (w1[1], w1[0], selector);
      c0[1] = hc_byte_perm_S (w1[0], w0[3], selector);
      c0[0] = hc_byte_perm_S (w0[3], w0[2], selector);
      w3[3] = hc_byte_perm_S (w0[2], w0[1], selector);
      w3[2] = hc_byte_perm_S (w0[1], w0[0], selector);
      w3[1] = hc_byte_perm_S (w0[0],     0, selector);
      w3[0] = 0;
      w2[3] = 0;
      w2[2] = 0;
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 14:
      c3[2] = hc_byte_perm_S (    0, w3[3], selector);
      c3[1] = hc_byte_perm_S (w3[3], w3[2], selector);
      c3[0] = hc_byte_perm_S (w3[2], w3[1], selector);
      c2[3] = hc_byte_perm_S (w3[1], w3[0], selector);
      c2[2] = hc_byte_perm_S (w3[0], w2[3], selector);
      c2[1] = hc_byte_perm_S (w2[3], w2[2], selector);
      c2[0] = hc_byte_perm_S (w2[2], w2[1], selector);
      c1[3] = hc_byte_perm_S (w2[1], w2[0], selector);
      c1[2] = hc_byte_perm_S (w2[0], w1[3], selector);
      c1[1] = hc_byte_perm_S (w1[3], w1[2], selector);
      c1[0] = hc_byte_perm_S (w1[2], w1[1], selector);
      c0[3] = hc_byte_perm_S (w1[1], w1[0], selector);
      c0[2] = hc_byte_perm_S (w1[0], w0[3], selector);
      c0[1] = hc_byte_perm_S (w0[3], w0[2], selector);
      c0[0] = hc_byte_perm_S (w0[2], w0[1], selector);
      w3[3] = hc_byte_perm_S (w0[1], w0[0], selector);
      w3[2] = hc_byte_perm_S (w0[0],     0, selector);
      w3[1] = 0;
      w3[0] = 0;
      w2[3] = 0;
      w2[2] = 0;
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 15:
      c3[3] = hc_byte_perm_S (    0, w3[3], selector);
      c3[2] = hc_byte_perm_S (w3[3], w3[2], selector);
      c3[1] = hc_byte_perm_S (w3[2], w3[1], selector);
      c3[0] = hc_byte_perm_S (w3[1], w3[0], selector);
      c2[3] = hc_byte_perm_S (w3[0], w2[3], selector);
      c2[2] = hc_byte_perm_S (w2[3], w2[2], selector);
      c2[1] = hc_byte_perm_S (w2[2], w2[1], selector);
      c2[0] = hc_byte_perm_S (w2[1], w2[0], selector);
      c1[3] = hc_byte_perm_S (w2[0], w1[3], selector);
      c1[2] = hc_byte_perm_S (w1[3], w1[2], selector);
      c1[1] = hc_byte_perm_S (w1[2], w1[1], selector);
      c1[0] = hc_byte_perm_S (w1[1], w1[0], selector);
      c0[3] = hc_byte_perm_S (w1[0], w0[3], selector);
      c0[2] = hc_byte_perm_S (w0[3], w0[2], selector);
      c0[1] = hc_byte_perm_S (w0[2], w0[1], selector);
      c0[0] = hc_byte_perm_S (w0[1], w0[0], selector);
      w3[3] = hc_byte_perm_S (w0[0],     0, selector);
      w3[2] = 0;
      w3[1] = 0;
      w3[0] = 0;
      w2[3] = 0;
      w2[2] = 0;
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;
  }
  #endif
}

DECLSPEC void switch_buffer_by_offset_be_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, PRIVATE_AS u32 *w2, PRIVATE_AS u32 *w3, const u32 offset)
{
  const int offset_switch = offset / 4;

  #if ((defined IS_AMD || defined IS_HIP) && HAS_VPERM == 0) || defined IS_GENERIC
  switch (offset_switch)
  {
    case  0:
      w3[3] = hc_bytealign_be_S (w3[2], w3[3], offset);
      w3[2] = hc_bytealign_be_S (w3[1], w3[2], offset);
      w3[1] = hc_bytealign_be_S (w3[0], w3[1], offset);
      w3[0] = hc_bytealign_be_S (w2[3], w3[0], offset);
      w2[3] = hc_bytealign_be_S (w2[2], w2[3], offset);
      w2[2] = hc_bytealign_be_S (w2[1], w2[2], offset);
      w2[1] = hc_bytealign_be_S (w2[0], w2[1], offset);
      w2[0] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w1[3] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w1[2] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w1[1] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w1[0] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w0[3] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w0[2] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w0[1] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w0[0] = hc_bytealign_be_S (    0, w0[0], offset);

      break;

    case  1:
      w3[3] = hc_bytealign_be_S (w3[1], w3[2], offset);
      w3[2] = hc_bytealign_be_S (w3[0], w3[1], offset);
      w3[1] = hc_bytealign_be_S (w2[3], w3[0], offset);
      w3[0] = hc_bytealign_be_S (w2[2], w2[3], offset);
      w2[3] = hc_bytealign_be_S (w2[1], w2[2], offset);
      w2[2] = hc_bytealign_be_S (w2[0], w2[1], offset);
      w2[1] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w2[0] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w1[3] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w1[2] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w1[1] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w1[0] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w0[3] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w0[2] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w0[1] = hc_bytealign_be_S (    0, w0[0], offset);
      w0[0] = 0;

      break;

    case  2:
      w3[3] = hc_bytealign_be_S (w3[0], w3[1], offset);
      w3[2] = hc_bytealign_be_S (w2[3], w3[0], offset);
      w3[1] = hc_bytealign_be_S (w2[2], w2[3], offset);
      w3[0] = hc_bytealign_be_S (w2[1], w2[2], offset);
      w2[3] = hc_bytealign_be_S (w2[0], w2[1], offset);
      w2[2] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w2[1] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w2[0] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w1[3] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w1[2] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w1[1] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w1[0] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w0[3] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w0[2] = hc_bytealign_be_S (    0, w0[0], offset);
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  3:
      w3[3] = hc_bytealign_be_S (w2[3], w3[0], offset);
      w3[2] = hc_bytealign_be_S (w2[2], w2[3], offset);
      w3[1] = hc_bytealign_be_S (w2[1], w2[2], offset);
      w3[0] = hc_bytealign_be_S (w2[0], w2[1], offset);
      w2[3] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w2[2] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w2[1] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w2[0] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w1[3] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w1[2] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w1[1] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w1[0] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w0[3] = hc_bytealign_be_S (    0, w0[0], offset);
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  4:
      w3[3] = hc_bytealign_be_S (w2[2], w2[3], offset);
      w3[2] = hc_bytealign_be_S (w2[1], w2[2], offset);
      w3[1] = hc_bytealign_be_S (w2[0], w2[1], offset);
      w3[0] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w2[3] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w2[2] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w2[1] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w2[0] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w1[3] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w1[2] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w1[1] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w1[0] = hc_bytealign_be_S (    0, w0[0], offset);
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  5:
      w3[3] = hc_bytealign_be_S (w2[1], w2[2], offset);
      w3[2] = hc_bytealign_be_S (w2[0], w2[1], offset);
      w3[1] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w3[0] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w2[3] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w2[2] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w2[1] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w2[0] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w1[3] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w1[2] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w1[1] = hc_bytealign_be_S (    0, w0[0], offset);
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  6:
      w3[3] = hc_bytealign_be_S (w2[0], w2[1], offset);
      w3[2] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w3[1] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w3[0] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w2[3] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w2[2] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w2[1] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w2[0] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w1[3] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w1[2] = hc_bytealign_be_S (    0, w0[0], offset);
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  7:
      w3[3] = hc_bytealign_be_S (w1[3], w2[0], offset);
      w3[2] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w3[1] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w3[0] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w2[3] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w2[2] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w2[1] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w2[0] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w1[3] = hc_bytealign_be_S (    0, w0[0], offset);
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  8:
      w3[3] = hc_bytealign_be_S (w1[2], w1[3], offset);
      w3[2] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w3[1] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w3[0] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w2[3] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w2[2] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w2[1] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w2[0] = hc_bytealign_be_S (    0, w0[0], offset);
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case  9:
      w3[3] = hc_bytealign_be_S (w1[1], w1[2], offset);
      w3[2] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w3[1] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w3[0] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w2[3] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w2[2] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w2[1] = hc_bytealign_be_S (    0, w0[0], offset);
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 10:
      w3[3] = hc_bytealign_be_S (w1[0], w1[1], offset);
      w3[2] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w3[1] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w3[0] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w2[3] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w2[2] = hc_bytealign_be_S (    0, w0[0], offset);
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 11:
      w3[3] = hc_bytealign_be_S (w0[3], w1[0], offset);
      w3[2] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w3[1] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w3[0] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w2[3] = hc_bytealign_be_S (    0, w0[0], offset);
      w2[2] = 0;
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 12:
      w3[3] = hc_bytealign_be_S (w0[2], w0[3], offset);
      w3[2] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w3[1] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w3[0] = hc_bytealign_be_S (    0, w0[0], offset);
      w2[3] = 0;
      w2[2] = 0;
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 13:
      w3[3] = hc_bytealign_be_S (w0[1], w0[2], offset);
      w3[2] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w3[1] = hc_bytealign_be_S (    0, w0[0], offset);
      w3[0] = 0;
      w2[3] = 0;
      w2[2] = 0;
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 14:
      w3[3] = hc_bytealign_be_S (w0[0], w0[1], offset);
      w3[2] = hc_bytealign_be_S (    0, w0[0], offset);
      w3[1] = 0;
      w3[0] = 0;
      w2[3] = 0;
      w2[2] = 0;
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;

    case 15:
      w3[3] = hc_bytealign_be_S (    0, w0[0], offset);
      w3[2] = 0;
      w3[1] = 0;
      w3[0] = 0;
      w2[3] = 0;
      w2[2] = 0;
      w2[1] = 0;
      w2[0] = 0;
      w1[3] = 0;
      w1[2] = 0;
      w1[1] = 0;
      w1[0] = 0;
      w0[3] = 0;
      w0[2] = 0;
      w0[1] = 0;
      w0[0] = 0;

      break;
  }
  #endif

}

