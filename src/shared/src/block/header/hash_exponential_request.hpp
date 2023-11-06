#pragma once
#include <cstdint>

#include "crypto/hash.hpp"
class HashExponentialDigest {
  friend struct Target;

public:
  uint32_t negExp{0}; // negative exponent of 2
  uint32_t data{0x80000000};

  HashExponentialDigest(){};
  HashExponentialDigest& digest(const Hash &);
};

inline HashExponentialDigest& HashExponentialDigest::digest(const Hash &h) {
  negExp += 1; // we are considering hashes as number in (0,1), padded with
               // infinite amount of trailing 1's
  size_t i = 0;
  for (; i < h.size(); ++i) {
    if (h[i] != 0)
      break;
    negExp += 8;
  }
  uint64_t tmpData{0};
  for (size_t j = 0;; ++j) {
    if (i < h.size())
      tmpData |= h[i++];
    else
      tmpData |= 0xFFu; // "infinite amount of trailing 1's"

    if (j >= 3)
      break;
    tmpData <<= 8;
  }
  size_t shifts = 0;
  while ((tmpData & 0x80000000ul) == 0) {
    shifts += 1;
    negExp += 1;
    tmpData <<= 1;
  }
  assert(shifts < 8);
  assert((tmpData >> 32) == 0);
  tmpData *= uint64_t(data);
  if (tmpData >= uint64_t(1) << 63) {
    tmpData >>= 1;
    negExp -= 1;
  }
  tmpData >>= 31;
  assert(tmpData < uint64_t(1) << 32);
  assert(tmpData >= uint64_t(1) << 31);
  data = tmpData;
  return *this;
};
