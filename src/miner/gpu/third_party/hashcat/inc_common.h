/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef INC_COMMON_H
#define INC_COMMON_H



// bit operations

DECLSPEC u32x hc_rotl32   (const u32x a, const int n);
DECLSPEC u32x hc_rotr32   (const u32x a, const int n);
DECLSPEC u32  hc_rotl32_S (const u32  a, const int n);
DECLSPEC u32  hc_rotr32_S (const u32  a, const int n);
DECLSPEC u64x hc_rotl64   (const u64x a, const int n);
DECLSPEC u64x hc_rotr64   (const u64x a, const int n);
DECLSPEC u64  hc_rotl64_S (const u64  a, const int n);
DECLSPEC u64  hc_rotr64_S (const u64  a, const int n);

DECLSPEC u32x hc_swap32   (const u32x v);
DECLSPEC u32  hc_swap32_S (const u32  v);
DECLSPEC u64x hc_swap64   (const u64x v);
DECLSPEC u64  hc_swap64_S (const u64  v);

// byte operations

DECLSPEC u32x hc_bytealign      (const u32x a, const u32x b, const int  c);
DECLSPEC u32  hc_bytealign_S    (const u32  a, const u32  b, const int  c);
DECLSPEC u32x hc_bytealign_be   (const u32x a, const u32x b, const int  c);
DECLSPEC u32  hc_bytealign_be_S (const u32  a, const u32  b, const int  c);
DECLSPEC u32x hc_byte_perm      (const u32x a, const u32x b, const int  c);
DECLSPEC u32  hc_byte_perm_S    (const u32  a, const u32  b, const int  c);

DECLSPEC u32x hc_add3           (const u32x a, const u32x b, const u32x c);
DECLSPEC u32  hc_add3_S         (const u32  a, const u32  b, const u32  c);
DECLSPEC u32x hc_bfe            (const u32x a, const u32x b, const u32x c);
DECLSPEC u32  hc_bfe_S          (const u32  a, const u32  b, const u32  c);
DECLSPEC u32x hc_lop_0x96       (const u32x a, const u32x b, const u32x c);
DECLSPEC u32  hc_lop_0x96_S     (const u32  a, const u32  b, const u32  c);

DECLSPEC int hc_enc_scan (PRIVATE_AS const u32 *buf, const int len);
DECLSPEC int hc_enc_scan_global (GLOBAL_AS const u32 *buf, const int len);
DECLSPEC void hc_enc_init (PRIVATE_AS hc_enc_t *hc_enc);
DECLSPEC int hc_enc_has_next (PRIVATE_AS hc_enc_t *hc_enc, const int sz);
DECLSPEC int hc_enc_next (PRIVATE_AS hc_enc_t *hc_enc, PRIVATE_AS const u32 *src_buf, const int src_len, const int src_sz, PRIVATE_AS u32 *dst_buf, const int dst_sz);
DECLSPEC int hc_enc_next_global (PRIVATE_AS hc_enc_t *hc_enc, GLOBAL_AS const u32 *src_buf, const int src_len, const int src_sz, PRIVATE_AS u32 *dst_buf, const int dst_sz);

DECLSPEC void append_0x80_4x4 (PRIVATE_AS u32x *w0, PRIVATE_AS u32x *w1, PRIVATE_AS u32x *w2, PRIVATE_AS u32x *w3, const u32 offset);
DECLSPEC void append_0x01_2x4_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, const u32 offset);
DECLSPEC void append_0x06_2x4_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, const u32 offset);
DECLSPEC void append_0x01_4x4_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, PRIVATE_AS u32 *w2, PRIVATE_AS u32 *w3, const u32 offset);
DECLSPEC void append_0x2d_4x4_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, PRIVATE_AS u32 *w2, PRIVATE_AS u32 *w3, const u32 offset);
DECLSPEC void append_0x80_1x4_S (PRIVATE_AS u32 *w0, const u32 offset);
DECLSPEC void append_0x80_2x4_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, const u32 offset);
DECLSPEC void append_0x80_3x4_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, PRIVATE_AS u32 *w2, const u32 offset);
DECLSPEC void append_0x80_4x4_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, PRIVATE_AS u32 *w2, PRIVATE_AS u32 *w3, const u32 offset);
DECLSPEC void append_0x80_8x4_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, PRIVATE_AS u32 *w2, PRIVATE_AS u32 *w3, PRIVATE_AS u32 *w4, PRIVATE_AS u32 *w5, PRIVATE_AS u32 *w6, PRIVATE_AS u32 *w7, const u32 offset);
DECLSPEC void make_utf16le (PRIVATE_AS const u32x *in, PRIVATE_AS u32x *out1, PRIVATE_AS u32x *out2);
DECLSPEC void make_utf16beN (PRIVATE_AS const u32x *in, PRIVATE_AS u32x *out1, PRIVATE_AS u32x *out2);
DECLSPEC void make_utf16be_S (PRIVATE_AS const u32 *in, PRIVATE_AS u32 *out1, PRIVATE_AS u32 *out2);
DECLSPEC void make_utf16le_S (PRIVATE_AS const u32 *in, PRIVATE_AS u32 *out1, PRIVATE_AS u32 *out2);
DECLSPEC void undo_utf16be_S (PRIVATE_AS const u32 *in1, PRIVATE_AS const u32 *in2, PRIVATE_AS u32 *out);
DECLSPEC void undo_utf16le_S (PRIVATE_AS const u32 *in1, PRIVATE_AS const u32 *in2, PRIVATE_AS u32 *out);
DECLSPEC void switch_buffer_by_offset_le_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, PRIVATE_AS u32 *w2, PRIVATE_AS u32 *w3, const u32 offset);
DECLSPEC void switch_buffer_by_offset_carry_le_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, PRIVATE_AS u32 *w2, PRIVATE_AS u32 *w3, PRIVATE_AS u32 *c0, PRIVATE_AS u32 *c1, PRIVATE_AS u32 *c2, PRIVATE_AS u32 *c3, const u32 offset);
DECLSPEC void switch_buffer_by_offset_be (PRIVATE_AS u32x *w0, PRIVATE_AS u32x *w1, PRIVATE_AS u32x *w2, PRIVATE_AS u32x *w3, const u32 offset);
DECLSPEC void switch_buffer_by_offset_be_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, PRIVATE_AS u32 *w2, PRIVATE_AS u32 *w3, const u32 offset);
DECLSPEC void switch_buffer_by_offset_carry_be_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, PRIVATE_AS u32 *w2, PRIVATE_AS u32 *w3, PRIVATE_AS u32 *c0, PRIVATE_AS u32 *c1, PRIVATE_AS u32 *c2, PRIVATE_AS u32 *c3, const u32 offset);
DECLSPEC void switch_buffer_by_offset_carry_be (PRIVATE_AS u32x *w0, PRIVATE_AS u32x *w1, PRIVATE_AS u32x *w2, PRIVATE_AS u32x *w3, PRIVATE_AS u32x *c0, PRIVATE_AS u32x *c1, PRIVATE_AS u32x *c2, PRIVATE_AS u32x *c3, const u32 offset);
DECLSPEC void switch_buffer_by_offset_8x4_le_S (PRIVATE_AS u32 *w0, PRIVATE_AS u32 *w1, PRIVATE_AS u32 *w2, PRIVATE_AS u32 *w3, PRIVATE_AS u32 *w4, PRIVATE_AS u32 *w5, PRIVATE_AS u32 *w6, PRIVATE_AS u32 *w7, const u32 offset);

#endif // INC_COMMON_H
