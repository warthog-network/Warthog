/* $Id: sph_sha2.h 216 2010-06-08 09:46:57Z tp $ */
/**
 * SHA-224, SHA-256, SHA-384 and SHA-512 interface.
 *
 * SHA-256 has been published in FIPS 180-2, now amended with a change
 * notice to include SHA-224 as well (which is a simple variation on
 * SHA-256). SHA-384 and SHA-512 are also defined in FIPS 180-2. FIPS
 * standards can be found at:
 *    http://csrc.nist.gov/publications/fips/
 *
 * ==========================(LICENSE BEGIN)============================
 *
 * Copyright (c) 2007-2010  Projet RNRT SAPHIR
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * ===========================(LICENSE END)=============================
 *
 * @file     sph_sha2.h
 * @author   Thomas Pornin <thomas.pornin@cryptolog.com>
 */

#ifndef SPH_SHA2_H__
#define SPH_SHA2_H__

#include <stddef.h>
#include <stdint.h>
#include "sph_types.h"

/**
 * Output size (in bits) for SHA-224.
 */
#define SPH_SIZE_sha224   224

/**
 * Output size (in bits) for SHA-256.
 */
#define SPH_SIZE_sha256   256

/**
 * This structure is a context for SHA-224 computations: it contains the
 * intermediate values and some data from the last entered block. Once
 * a SHA-224 computation has been performed, the context can be reused for
 * another computation.
 *
 * The contents of this structure are private. A running SHA-224 computation
 * can be cloned by copying the context (e.g. with a simple
 * <code>memcpy()</code>).
 */
typedef struct {
#ifndef DOXYGEN_IGNORE
	unsigned char buf[64];    /* first field, for alignment */
	sph_u32 val[8];
#if SPH_64
	sph_u64 count;
#else
	sph_u32 count_high, count_low;
#endif
#endif
} sph_sha224_context __attribute__((aligned(64)));

/**
 * This structure is a context for SHA-256 computations. It is identical
 * to the SHA-224 context. However, a context is initialized for SHA-224
 * <strong>or</strong> SHA-256, but not both (the internal IV is not the
 * same).
 */
typedef sph_sha224_context sph_sha256_context;


/**
 * Initialize a SHA-256 context. This process performs no memory allocation.
 *
 * @param cc   the SHA-256 context (pointer to
 *             a <code>sph_sha256_context</code>)
 */
void sph_sha256_init(void *cc);

#ifdef DOXYGEN_IGNORE
/**
 * Process some data bytes, for SHA-256. This function is identical to
 * <code>sha_224()</code>
 *
 * @param cc     the SHA-224 context
 * @param data   the input data
 * @param len    the input data length (in bytes)
 */
void sph_sha256(void *cc, const void *data, size_t len);
#endif

#ifndef DOXYGEN_IGNORE
#define sph_sha256   sph_sha224
#endif

/**
 * Terminate the current SHA-256 computation and output the result into the
 * provided buffer. The destination buffer must be wide enough to
 * accomodate the result (32 bytes). The context is automatically
 * reinitialized.
 *
 * @param cc    the SHA-256 context
 * @param dst   the destination buffer
 */
void sph_sha256_close(void *cc, void *dst);

/**
 * Add a few additional bits (0 to 7) to the current computation, then
 * terminate it and output the result in the provided buffer, which must
 * be wide enough to accomodate the result (32 bytes). If bit number i
 * in <code>ub</code> has value 2^i, then the extra bits are those
 * numbered 7 downto 8-n (this is the big-endian convention at the byte
 * level). The context is automatically reinitialized.
 *
 * @param cc    the SHA-256 context
 * @param ub    the extra bits
 * @param n     the number of extra bits (0 to 7)
 * @param dst   the destination buffer
 */
void sph_sha256_addbits_and_close(void *cc, unsigned ub, unsigned n, void *dst);

#ifdef DOXYGEN_IGNORE
/**
 * Apply the SHA-256 compression function on the provided data. This
 * function is identical to <code>sha224_comp()</code>.
 *
 * @param msg   the message block (16 values)
 * @param val   the function 256-bit input and output
 */
void sph_sha256_comp(const sph_u32 msg[16], sph_u32 val[8]);
#endif

#ifndef DOXYGEN_IGNORE
#define sph_sha256_comp   sph_sha224_comp
#endif

// void sph_sha256_full( void *dst, const void *data, size_t len );

// These shouldn't be called directly, use sha256-hash.h generic functions
// sha256_transform_le & sha256_transform_be instead.
void sha256_transform_le( uint32_t *state_out, const uint32_t *data,
                              const uint32_t *state_in );

void sha256_transform_be( uint32_t *state_out, const uint32_t *data,
                              const uint32_t *state_in );

#endif
