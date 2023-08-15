/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef INC_TYPES_H
#define INC_TYPES_H


#ifdef IS_METAL
typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned int   uint;
typedef unsigned long  ulong;
#define ullong ulong
#endif

#ifdef IS_OPENCL
typedef ulong   ullong;
typedef ulong2  ullong2;
typedef ulong4  ullong4;
typedef ulong8  ullong8;
typedef ulong16 ullong16;
#endif

#ifdef KERNEL_STATIC
typedef uchar  u8;
typedef ushort u16;
typedef uint   u32;
#ifdef IS_METAL
typedef ulong  u64;
#else
typedef ullong u64;
#endif
#else
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#endif

typedef u8  u8a;
typedef u16 u16a;
typedef u32 u32a;
typedef u64 u64a;

#ifndef NEW_SIMD_CODE
#undef  VECT_SIZE
#define VECT_SIZE 1
#endif

#define CONCAT(a, b)       a##b
#define VTYPE(type, width) CONCAT(type, width)

// emulated is always VECT_SIZE = 1
#if VECT_SIZE == 1
typedef u8   u8x;
typedef u16  u16x;
typedef u32  u32x;
typedef u64  u64x;

#define make_u8x  (u8)
#define make_u16x (u16)
#define make_u32x (u32)
#define make_u64x (u64)

#else

typedef VTYPE(uchar,  VECT_SIZE) u8x;
typedef VTYPE(ushort, VECT_SIZE) u16x;
typedef VTYPE(uint,   VECT_SIZE) u32x;
typedef VTYPE(ullong, VECT_SIZE) u64x;

#ifndef IS_METAL
#define make_u8x  (u8x)
#define make_u16x (u16x)
#define make_u32x (u32x)
#define make_u64x (u64x)
#else
#define make_u8x  u8x
#define make_u16x u16x
#define make_u32x u32x
#define make_u64x u64x
#endif

#endif

// unions

typedef union vconv32
{
  u64 v32;

  struct
  {
    u16 a;
    u16 b;

  } v16;

  struct
  {
    u8 a;
    u8 b;
    u8 c;
    u8 d;

  } v8;

} vconv32_t;

typedef union vconv64
{
  u64 v64;

  struct
  {
    u32 a;
    u32 b;

  } v32;

  struct
  {
    u16 a;
    u16 b;
    u16 c;
    u16 d;

  } v16;

  struct
  {
    u8 a;
    u8 b;
    u8 c;
    u8 d;
    u8 e;
    u8 f;
    u8 g;
    u8 h;

  } v8;

} vconv64_t;

/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */



typedef enum sha2_32_constants
{
  // SHA-256 Initial Hash Values
  SHA256M_A=0x6a09e667U,
  SHA256M_B=0xbb67ae85U,
  SHA256M_C=0x3c6ef372U,
  SHA256M_D=0xa54ff53aU,
  SHA256M_E=0x510e527fU,
  SHA256M_F=0x9b05688cU,
  SHA256M_G=0x1f83d9abU,
  SHA256M_H=0x5be0cd19U,

  // SHA-256 Constants
  SHA256C00=0x428a2f98U,
  SHA256C01=0x71374491U,
  SHA256C02=0xb5c0fbcfU,
  SHA256C03=0xe9b5dba5U,
  SHA256C04=0x3956c25bU,
  SHA256C05=0x59f111f1U,
  SHA256C06=0x923f82a4U,
  SHA256C07=0xab1c5ed5U,
  SHA256C08=0xd807aa98U,
  SHA256C09=0x12835b01U,
  SHA256C0a=0x243185beU,
  SHA256C0b=0x550c7dc3U,
  SHA256C0c=0x72be5d74U,
  SHA256C0d=0x80deb1feU,
  SHA256C0e=0x9bdc06a7U,
  SHA256C0f=0xc19bf174U,
  SHA256C10=0xe49b69c1U,
  SHA256C11=0xefbe4786U,
  SHA256C12=0x0fc19dc6U,
  SHA256C13=0x240ca1ccU,
  SHA256C14=0x2de92c6fU,
  SHA256C15=0x4a7484aaU,
  SHA256C16=0x5cb0a9dcU,
  SHA256C17=0x76f988daU,
  SHA256C18=0x983e5152U,
  SHA256C19=0xa831c66dU,
  SHA256C1a=0xb00327c8U,
  SHA256C1b=0xbf597fc7U,
  SHA256C1c=0xc6e00bf3U,
  SHA256C1d=0xd5a79147U,
  SHA256C1e=0x06ca6351U,
  SHA256C1f=0x14292967U,
  SHA256C20=0x27b70a85U,
  SHA256C21=0x2e1b2138U,
  SHA256C22=0x4d2c6dfcU,
  SHA256C23=0x53380d13U,
  SHA256C24=0x650a7354U,
  SHA256C25=0x766a0abbU,
  SHA256C26=0x81c2c92eU,
  SHA256C27=0x92722c85U,
  SHA256C28=0xa2bfe8a1U,
  SHA256C29=0xa81a664bU,
  SHA256C2a=0xc24b8b70U,
  SHA256C2b=0xc76c51a3U,
  SHA256C2c=0xd192e819U,
  SHA256C2d=0xd6990624U,
  SHA256C2e=0xf40e3585U,
  SHA256C2f=0x106aa070U,
  SHA256C30=0x19a4c116U,
  SHA256C31=0x1e376c08U,
  SHA256C32=0x2748774cU,
  SHA256C33=0x34b0bcb5U,
  SHA256C34=0x391c0cb3U,
  SHA256C35=0x4ed8aa4aU,
  SHA256C36=0x5b9cca4fU,
  SHA256C37=0x682e6ff3U,
  SHA256C38=0x748f82eeU,
  SHA256C39=0x78a5636fU,
  SHA256C3a=0x84c87814U,
  SHA256C3b=0x8cc70208U,
  SHA256C3c=0x90befffaU,
  SHA256C3d=0xa4506cebU,
  SHA256C3e=0xbef9a3f7U,
  SHA256C3f=0xc67178f2U,

} sha2_32_constants_t;



#ifdef KERNEL_STATIC
typedef struct digest
{
  u32 digest_buf[DGST_ELEM];

} digest_t;
#endif






typedef struct hc_enc
{
  int  pos;   // source offset

  u32  cbuf;  // carry buffer
  int  clen;  // carry length

} hc_enc_t;

#endif
