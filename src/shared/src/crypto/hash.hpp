#pragma once
#include "general/view.hpp"
#include <array>
#include <cstring>
#include "wrt/optional.hpp"
#include <string>

class Hash;
class HashView : public View<32> {
public:
    inline HashView(const uint8_t* pos)
        : View<32>(pos) { };
    inline bool operator==(HashView hv) const
    {
        return View::operator==(hv);
    };
    HashView(const Hash&);
};

class Hash : public std::array<uint8_t, 32> {
    Hash() = default;

public:
    static constexpr size_t byte_size() { return 32; }
    static wrt::optional<Hash> parse_string(std::string_view);
    static Hash uninitialized()
    {
        return {};
    }
    Hash(std::array<uint8_t, 32> other)
        : array(std::move(other))
    {
    }
    Hash(const Hash&) = default;
    Hash(Hash&&) = default;
    std::string hex_string() const;
    explicit Hash(HashView hv)
    {
        memcpy(data(), hv.data(), 32);
    }
    Hash& operator=(const Hash&) = default;
    bool operator==(const Hash&) const = default;
    bool operator!=(const Hash&) const = default;
};

inline HashView::HashView(const Hash& h)
    : HashView(h.data())
{
}

inline bool operator==(const Hash& h, const HashView& hv)
{
    return (HashView(h) == hv);
};
inline bool operator==(const HashView& hv, const Hash& h)
{
    return (HashView(h) == hv);
};

template <typename T>
class GenericHash : public Hash {
public:
    explicit GenericHash(Hash h)
        : Hash(std::move(h))
    {
    }
    explicit GenericHash(HashView h)
        : Hash(h)
    {
    }
    explicit GenericHash(std::array<uint8_t, 32> other)
        : Hash(std::move(other))
    {
    }
    [[nodiscard]] static wrt::optional<T> parse_string(std::string_view s)
    {
        auto p { Hash::parse_string(s) };
        if (p)
            return T { *p };
        return {};
    }
    static T uninitialized()
    {
        return T { Hash::uninitialized() };
    }
};

class BlockHash : public GenericHash<BlockHash> {
public:
    using GenericHash::GenericHash;
    static BlockHash genesis();
};

class AssetHash : public GenericHash<AssetHash> {
public:
    using GenericHash::GenericHash;
};

class TxHash : public GenericHash<TxHash> {
public:
    using GenericHash::GenericHash;
};

class PinHash : public GenericHash<PinHash> {
public:
    using GenericHash::GenericHash;
};
