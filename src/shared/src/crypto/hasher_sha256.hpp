#pragma once

#include "general/with_uint64.hpp"
#include "hash.hpp"
#include "sha2.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
class HasherSHA256 {
private:
    SHA256_CTX ctx;

public:
    HasherSHA256()
    {
        sha256_Init(&ctx);
    }
    template <size_t N>
    HasherSHA256&& operator<<(const std::array<uint8_t, N>& arr) &&
    {
        return std::move(write(arr.data(), arr.size()));
    }

    HasherSHA256&& operator<<(const std::vector<uint8_t>& v) &&
    {
        return std::move(write(v.data(), v.size()));
    }
    template <size_t N>
    HasherSHA256&& operator<<(View<N> v) &&
    {
        return std::move(*this).write(v.data(), v.size());
    }
    HasherSHA256&& operator<<(IsUint32 val) &&
    {
        return std::move(*this).operator<<(val.value());
    }
    HasherSHA256&& operator<<(uint32_t val) &&
    {
        uint32_t valBe = hton32(val);
        return std::move(write(&valBe, 4));
    }
    HasherSHA256&& operator<<(IsUint64 val) &&
    {
        return std::move(*this).operator<<(val.value());
    }
    HasherSHA256&& operator<<(uint64_t val) &&
    {
        uint64_t valBe = hton64(val);
        return std::move(write(&valBe, 8));
    }
    HasherSHA256&& operator<<(uint16_t val) &&
    {
        uint16_t valBe = hton16(val);
        return std::move(write(&valBe, 2));
    }
    operator Hash() &&
    {
        Hash tmp;
        finalize(tmp.data());
        return tmp;
    }

    HasherSHA256&& write(const void* data, size_t len)
    {
        sha256_Update(&ctx, (uint8_t*)data, len);
        return std::move(*this);
    }

private:
    void finalize(uint8_t* out256)
    {
        sha256_Final(&ctx, out256);
    }
};

inline Hash hashSHA256(const uint8_t* data, size_t len)
{
    Hash res;
    sha256_Raw(data, len, res.data());
    return res;
}

inline Hash hashSHA256(const std::vector<uint8_t>& vec)
{
    return hashSHA256(vec.data(), vec.size());
}

template <size_t N>
inline Hash hashSHA256(const std::array<uint8_t, N>& arr)
{
    return hashSHA256(arr.data(), arr.size());
}

constexpr std::array<uint8_t, 32> zerohash = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
