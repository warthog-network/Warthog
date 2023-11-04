#pragma once
#include "crypto/hash.hpp"
#include "crypto/verushash/verushash.hpp"
#include "crypto/verushash/verus_constants.hpp"
#include <cstdint>
#include <optional>
#include <span>

void haraka256(unsigned char *out, const unsigned char *in);
void haraka512(unsigned char *out, const unsigned char *in);
void haraka512_keyed(unsigned char *out, const unsigned char *in,
                     const u128 *rc);
uint64_t verusclhash_sv2_1(void *random, const unsigned char buf[64],
                           uint64_t keyMask, __m128i **pMoveScratch);
namespace Verus {
class MinerOpt {
  struct Success {
    Hash hash;
    std::array<uint8_t, 80> arg;
  };

public:
  MinerOpt(std::span<uint8_t, 80> arg, HashView target, uint32_t seedOffset);
  void set_target(HashView newTarget) { target = newTarget; }
  void set_offset(uint32_t newSeedOffset) { seedOffset = newSeedOffset; }
  void set_header(std::span<uint8_t, 80> arg);
  [[nodiscard]]std::optional<Success> mine(uint32_t count);
  [[nodiscard]] uint32_t last_count() const{return lastCount;}

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
