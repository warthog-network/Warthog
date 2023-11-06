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

namespace Verus {
class MinerPort {
  struct Success {
    Hash hash;
    std::array<uint8_t, 80> arg;
  };

public:
  MinerPort(std::span<uint8_t, 80> arg, HashView target, uint32_t seedOffset);
  void set_target(HashView newTarget) { target = newTarget; }
  void set_offset(uint32_t newSeedOffset) { seedOffset = newSeedOffset; }
  void set_header(std::span<uint8_t, 80> arg);
  [[nodiscard]] std::optional<Success> mine(uint32_t count);
  [[nodiscard]] uint32_t last_count() const { return lastCount; }

private:
  VerusHasher vh;
  alignas(32) uint8_t key[2 * keySizeInBytes];
  alignas(4) std::array<uint8_t, 80> arg;
  Hash target;
  Hash curSeed;
  uint32_t seedOffset{0};
  size_t lastCount{0};
};
} // namespace Verus
