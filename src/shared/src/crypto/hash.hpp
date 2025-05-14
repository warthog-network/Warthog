#pragma once
#include "general/view.hpp"
#include <array>
#include <cstring>
#include <optional>
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
    static std::optional<Hash> parse_string(std::string_view);
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
    static Hash genesis();
};

inline HashView::HashView(const Hash& h)
    : HashView(h.data())
{
}

class TokenHash : public Hash {
    using Hash::Hash;
};

inline bool operator==(const Hash& h, const HashView& hv)
{
    return (HashView(h) == hv);
};
inline bool operator==(const HashView& hv, const Hash& h)
{
    return (HashView(h) == hv);
};

class TxHash : public Hash {
public:
    explicit TxHash(Hash h)
        : Hash(h)
    {
    }
};
