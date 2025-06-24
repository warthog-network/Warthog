#pragma once

#include "general/serializer.hxx"
#include "general/with_uint64.hpp"

#include "hash.hpp"
#include "sha2.hpp"
#include <array>
#include <cstdint>

class HasherSHA256 {
private:
    SHA256_CTX ctx;

public:
    HasherSHA256()
    {
        sha256_Init(&ctx);
    }

    operator Hash() &&
    {
        Hash tmp { Hash::uninitialized() };
        finalize(tmp.data());
        return tmp;
    }

    void write(const std::span<const uint8_t>& s)
    {
        sha256_Update(&ctx, s.data(), s.size());
    }

private:
    void finalize(uint8_t* out256)
    {
        sha256_Final(&ctx, out256);
    }
};

inline Hash hashSHA256(std::span<const uint8_t> s)
{
    return HasherSHA256() << s;
}

inline Hash hashSHA256(const uint8_t* data, size_t len)
{
    return hashSHA256({ data, len });
}

template <typename... Ts>
[[nodiscard]] inline Hash hash_args_SHA256(Ts&&... ts)
{
    return (HasherSHA256() << ... << std::forward<Ts>(ts));
}

template <typename T>
Hash hashSHA256(T&& t)
{
    return HasherSHA256() << std::forward<T>(t);
}

template <size_t N>
inline Hash hashSHA256(const std::array<uint8_t, N>& arr)
{
    return hashSHA256(arr.data(), arr.size());
}
