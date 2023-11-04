#pragma once
#include <cstdint>
namespace Verus{

constexpr uint64_t keymask(uint64_t keysize) {
  int i = 0;
  while (keysize >>= 1) {
    i++;
  }
  return i ? (((uint64_t)1) << i) - 1 : 0;
}
static constexpr uint64_t keysize = 1024 * 8 + (40 * 16);
static constexpr uint64_t keySizeInBytes = (keysize >> 5) << 5;
static constexpr uint64_t keyMask{keymask(keysize)};
static constexpr uint64_t keyMask16{keyMask >> 4};
static constexpr uint64_t keyRefreshsize{keyMask + 1};
static constexpr uint64_t key256blocks{keySizeInBytes >> 5};
static constexpr uint64_t key256extra{keySizeInBytes & 0x1f};
}
