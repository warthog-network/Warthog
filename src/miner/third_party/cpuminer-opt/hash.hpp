#pragma once
#include <cstdint>
#include <cstdlib>
#include <byteswap.h>
#include <cstring>

extern "C" {
void sha256_transform_le(uint32_t *state_out, const void *input,
                         const uint32_t *state_in);
void sha256_transform_be(uint32_t *state_out, const void *input,
                         const uint32_t *state_in);
}

typedef struct {
  unsigned char buf[64]; /* first field, for alignment */
  uint32_t state[8];
  uint64_t count;
} sha256_context __attribute__((aligned(64)));

static const uint32_t SHA256_IV[8] = {0x6A09E667, 0xBB67AE85, 0x3C6EF372,
                                      0xA54FF53A, 0x510E527F, 0x9B05688C,
                                      0x1F83D9AB, 0x5BE0CD19};

inline void sha256_ctx_init(sha256_context *ctx) {
  memcpy(ctx->state, SHA256_IV, sizeof SHA256_IV);
  ctx->count = 0;
}

inline void sha256_update(sha256_context *ctx, const uint8_t *data,
                          size_t len) {
  int ptr = ctx->count & 0x3f;
  const uint8_t *src = data;

  ctx->count += (uint64_t)len;

  if (len < 64u - ptr) {
    memcpy(ctx->buf + ptr, src, len);
    return;
  }

  memcpy(ctx->buf + ptr, src, 64 - ptr);
  sha256_transform_be(ctx->state, (uint32_t *)ctx->buf, ctx->state);
  src += 64 - ptr;
  len -= 64 - ptr;

  while (len >= 64) {
    sha256_transform_be(ctx->state, (uint32_t *)src, ctx->state);
    src += 64;
    len -= 64;
  }

  memcpy(ctx->buf, src, len);
}

inline void sha256_final(sha256_context *ctx, void *hash) {
  int ptr = ctx->count & 0x3f;

  ctx->buf[ptr++] = 0x80;

  if (ptr > 56) {
    memset(ctx->buf + ptr, 0, 64 - ptr);
    sha256_transform_be(ctx->state, (uint32_t *)ctx->buf, ctx->state);
    memset(ctx->buf, 0, 56);
  } else
    memset(ctx->buf + ptr, 0, 56 - ptr);

  *(uint64_t *)(&ctx->buf[56]) = bswap_64(ctx->count << 3);

  sha256_transform_be(ctx->state, (uint32_t *)ctx->buf, ctx->state);

  for (int i = 0; i < 8; i++)
    ((uint32_t *)hash)[i] = bswap_32(ctx->state[i]);
}

inline void sha256_full(void *hash, const uint8_t *data, size_t len) {
  sha256_context ctx;
  sha256_ctx_init(&ctx);
  sha256_update(&ctx, data, len);
  sha256_final(&ctx, hash);
}

inline void sha256d(uint8_t *hash, const uint8_t *data, int len) {
  sha256_full(hash, data, len);
  sha256_full(hash, hash, 32);
}
