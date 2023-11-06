#pragma once

#include "crypto/hash.hpp"
#include "u128.h"
#include <span>

namespace Verus {
bool can_optimize();

class MinerOpt;

class VerusHasher {
    friend class MinerOpt;
    friend class MinerPort;

public:
    VerusHasher();
    void reset()
    {
        std::memset(curBuf, 0, 64);
        curPos = 0;
    };
    VerusHasher& write(const uint8_t* data, size_t len);
    VerusHasher& write(std::span<const uint8_t> s)
    {
        return write(s.data(), s.size());
    }
    [[nodiscard]] Hash finalize();

private:
    // data
    VerusHasher& write(const uint8_t* data, const size_t len,
        void (*haraka512)(unsigned char* out,
            const unsigned char* in))
    {
        // digest up to 32 bytes at a time
        for (size_t pos = 0; pos < len;) {
            size_t room = 32 - curPos;

            if (len - pos >= room) {
                memcpy(curBuf + 32 + curPos, data + pos, room);
                haraka512(result, curBuf);
                std::swap(result, curBuf);
                pos += room;
                curPos = 0;
            } else {
                memcpy(curBuf + 32 + curPos, data + pos, len - pos);
                curPos += len - pos;
                pos = len;
            }
        }
        return *this;
    }
    alignas(32) unsigned char buf1[64] = { 0 }, buf2[64];
    unsigned char *curBuf = buf1, *result = buf2;
    size_t curPos = 0;
    // Hash hasher_seed;
    bool optimized { false };

    // methods
    template <typename T>
    void FillExtra(const T* _data);
};
} // namespace Verus

[[nodiscard]] inline Hash verus_hash(std::span<const uint8_t> s) // verushash v2.1
{
    return Verus::VerusHasher().write(s).finalize();
}
