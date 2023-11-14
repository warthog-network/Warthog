#pragma once
#include "crypto/hash.hpp"
#include "u128.h"
#include "verus_constants.hpp"
#include "verushash.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
uint64_t verusclhash_sv2_1_port(void *random, const unsigned char buf[64],
                                uint64_t keyMask, __m128i **pMoveScratch);

void haraka512_port(unsigned char *out, const unsigned char *in);

/* Implementation of Haraka-512 */
void haraka512_port_keyed(unsigned char *out, const unsigned char *in,
                          const u128 *rc);

/* Implementation of Haraka-256 */
void haraka256_port(unsigned char *out, const unsigned char *in);

