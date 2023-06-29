#include "sndbuffer.hpp"
#include "crypto/hasher_sha256.hpp"

void Sndbuffer::writeChecksum()
{
    auto tmp = hashSHA256(reinterpret_cast<uint8_t*>(ptr.get() + 8), len - 8);
    memcpy(ptr.get() + 4, tmp.data(), 4);
};
