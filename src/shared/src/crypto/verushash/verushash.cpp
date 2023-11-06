#ifdef _WIN32
#undef __cpuid
#include <intrin.h>
#endif

#if defined(__arm__) || defined(__aarch64__)
#include "crypto/sse2neon.h"
#include <asm/hwcap.h>
#include <sys/auxv.h>
#else
#include <cpuid.h>
#include <x86intrin.h>
#endif // !WIN32

// #include "verus_clhash_opt.hpp"
#include "verus_clhash_port.hpp"

namespace Verus {
class HashKey {
public:
    using VerusHashFunctionType = uint64_t (*)(void* random,
        const unsigned char buf[64],
        uint64_t keyMask,
        __m128i** pMoveScratch);
    HashKey(HashView seedBytes,
        void (*haraka256Function)(unsigned char* out,
            const unsigned char* in));
    void update_seed(HashView newSeed,
        void (*haraka256Function)(unsigned char* out,
            const unsigned char* in));

    inline __m128i** getpmovescratch()
    {
        return (__m128i**)(hasherrefresh() + keyRefreshsize);
    }
    inline uint8_t* hasherrefresh() { return key + keySizeInBytes; }
    inline void fixupkey()
    {
        constexpr uint32_t ofs = keySizeInBytes >> 4;
        __m128i** ppfixup = getpmovescratch(); // past the part to refresh from
        for (__m128i* pfixup = *ppfixup; pfixup; pfixup = *++ppfixup) {
            *pfixup = *(pfixup + ofs); // we hope the compiler cancels this operation out before add
        }
    }
    uint64_t apply_verusclhash(uint8_t* curBuf, VerusHashFunctionType f)
    {
        return f(
            key, curBuf, keyMask,
            (__m128i**)((unsigned char*)key + (keySizeInBytes + keyRefreshsize)));
    }

    // this prepares a key for hashing and mutation by copying it from the
    // original key for this block WARNING!! this does not check for NULL ptr, so
    // make sure the buffer is allocated
    inline void* gethashkey()
    {
        fixupkey();
        return key;
    }

    const uint8_t* key_data() { return key; }

private:
    Hash curSeed;
    void apply_seed(HashView newSeed,
        void (*haraka256Function)(unsigned char* out,
            const unsigned char* in));
    /* data */
    alignas(32) uint8_t key[2 * keySizeInBytes];
};
bool can_optimize()
{
#if defined(__arm__) || defined(__aarch64__)
    long hwcaps = getauxval(AT_HWCAP);

    if ((hwcaps & HWCAP_AES) && (hwcaps & HWCAP_PMULL))
        return true;
    else
        return false;
#else
    unsigned int eax, ebx, ecx, edx;
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        return false;
    } else {
        return ((ecx & (bit_AVX | bit_AES | bit_PCLMUL)) == (bit_AVX | bit_AES | bit_PCLMUL));
    }
#endif
}

VerusHasher::VerusHasher()
{
    optimized = can_optimize();
}

template <typename T>
void VerusHasher::FillExtra(const T* _data)
{
    uint8_t* data = (unsigned char*)_data;
    size_t pos = curPos;
    size_t left = 32 - pos;
    do {
        int len = left > sizeof(T) ? sizeof(T) : left;
        std::memcpy(curBuf + 32 + pos, data, len);
        pos += len;
        left -= len;
    } while (left > 0);
}
// void VerusHasher::haraka512KeyedFunction(unsigned char* out,
//     const unsigned char* in,
//     const u128* rc)
// {
//     if (optimized)
//         return haraka512_keyed(out, in, rc);
//     return haraka512_port_keyed(out, in, rc);
// };
// uint64_t VerusHasher::verusclhashfunction(void* random,
//     const unsigned char buf[64],
//     uint64_t keyMask,
//     __m128i** pMoveScratch)
// {
//
//     if (optimized)
//         return verusclhash_sv2_1(random, buf, keyMask, pMoveScratch);
//     return verusclhash_sv2_1_port(random, buf, keyMask, pMoveScratch);
// };

void HashKey::update_seed(HashView newSeed,
    void (*haraka256Function)(unsigned char* out,
        const unsigned char* in))
{
    if (newSeed != curSeed) {
        apply_seed(newSeed, haraka256Function);
    } else {
        memcpy(key, key + keySizeInBytes, keyRefreshsize);
        memset((unsigned char*)key + (keySizeInBytes + keyRefreshsize), 0,
            keySizeInBytes - keyRefreshsize);
    }
};
void HashKey::apply_seed(HashView newSeed,
    void (*haraka256Function)(unsigned char* out,
        const unsigned char* in))
{
    unsigned char* pkey = key;
    const unsigned char* psrc = newSeed.data();
    for (size_t i = 0; i < key256blocks; i++) {
        (*haraka256Function)(pkey, psrc);
        psrc = pkey;
        pkey += 32;
    }
    if (key256extra != 0) {
        unsigned char buf[32];
        (*haraka256Function)(buf, psrc);
        memcpy(pkey, buf, key256extra);
    }
    memcpy(key + keySizeInBytes, key, keyRefreshsize);
    memset((unsigned char*)key + (keySizeInBytes + keyRefreshsize), 0,
        keySizeInBytes - keyRefreshsize);
};
HashKey::HashKey(HashView seed,
    void (*haraka256Function)(unsigned char* out,
        const unsigned char* in))
    : curSeed(seed)
{
    apply_seed(seed, haraka256Function);
}

Hash VerusHasher::finalize()
{
    // fill buffer to the end with the beginning of it to prevent any
    // foreknowledge of bits that may contain zero
    FillExtra((u128*)curBuf);

    // gen new key with what is last in buffer

    HashKey hk(curBuf,  haraka256_port);

    // run verusclhash on the buffer
    uint64_t intermediate { hk.apply_verusclhash(
        curBuf,  verusclhash_sv2_1_port) };
    // fill buffer to the end with the result
    FillExtra(&intermediate);

    // get the final hash with a mutated dynamic key for each hash result
    Hash out;
    constexpr uint64_t mask16 = keyMask >> 4;

    haraka512_port_keyed(out.data(), curBuf,
            (const u128*)hk.key_data() + (intermediate & mask16));

    return out;
};

VerusHasher& VerusHasher::write(const uint8_t* data, size_t len)
{
    return write(data, len, haraka512_port);
};

} // namespace Verus
