
#ifdef KERNEL_STATIC
#include "hashcat/inc_common.cl"
#include "hashcat/inc_hash_sha256.cl"
#include "hashcat/inc_platform.cl"
#include "hashcat/inc_types.h"
#include "hashcat/inc_vendor.h"
#endif

// global variables
volatile __global u32 g_counter = 0;
volatile __global u32 target;
volatile __global sha256_ctx_t prepared_ctx;



// helper functions
inline u8 at0(u32 d) { return d >> 24; }
inline u8 at1(u32 d) { return d >> 16; }
inline u8 at2(u32 d) { return d >> 8; }
inline u8 at3(u32 d) { return d; }
inline u32 bits(u32 d) { return 0x00FFFFFFul & d; }
inline bool compatible(u32 target, const u8 *hash) {
  u8 zeros = at0(target);
  if (zeros > (256u - 4 * 8u))
    return false;
  if ((at1(target) & 0x80) == 0)
    return false;                     // first digit must be 1
  const size_t zerobytes = zeros / 8; // number of complete zero bytes
  const size_t shift = zeros & 0x07u;

  for (size_t i = 0; i < zerobytes; ++i)
    if (hash[31 - i] != 0u)
      return false; // here we need zeros

  u32 threshold = bits(target) << (8u - shift);
  u32 candidate;
  u8 *dst = (u8 *)&candidate;
  const u8 *src = &hash[28 - zerobytes];
  dst[0] = src[3];
  dst[1] = src[2];
  dst[2] = src[1];
  dst[3] = src[0];
  candidate = hc_swap32(candidate);
  if (candidate > threshold) {
    return false;
  }
  if (candidate < threshold) {
    return true;
  }
  for (size_t i = 0; i < 28 - zerobytes; ++i)
    if (hash[i] != 0)
      return false;
  return true;
}
bool valid_hash(u32 *hash32) {
  return compatible(target, (u8 *)hash32);
}

// kernel functions
void kernel reset_counter(global u32 *C) {
  C[0] = g_counter;
  g_counter = 0;
}

void kernel set_target(u32 t) { target = hc_swap32(t); }

void kernel set_block_header(const global u32 *data76) {
  u32 buf[32] = {0};
  for (int i1 = 0, i4 = 0; i4 < 76; ++i1, i4 += 4)
    buf[i1] = hc_swap32(data76[i1]);
  sha256_init(&prepared_ctx);
  sha256_update(&prepared_ctx, buf, 76);
}

int synced_counter() { return atomic_inc(&g_counter); }
void kernel mine(global u32 *args, global u32 *hashes) {
  const u32 gid = get_global_id(0);
  const u32 lid = get_local_id(0);

  sha256_ctx_t ctx = prepared_ctx;
  ctx.len += 4;
  ctx.w0[3] = gid;
  sha256_final(&ctx);
  sha256_ctx_t ctx2;
  sha256_init(&ctx2);
  u32 a[16] = {0};
  for (int i = 0; i < 8; ++i)
    a[i] = ctx.h[i];
  sha256_update(&ctx2, a, 32);
  sha256_final(&ctx2);
  u32 hash[8];
  for (int i = 0; i < 8; ++i)
    hash[i] = hc_swap32(ctx2.h[i]);
  if (valid_hash(hash)) {
    int c = synced_counter();
    if (c < 8) {
      args[c] = gid;
      for (int i = 0; i < 8; ++i)
        hashes[8 * c + i] = hash[i];
    }
  }
}
