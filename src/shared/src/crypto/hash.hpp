#pragma once
#include "general/params.hpp"
#include "general/view.hpp"
#include <array>
#include <cstring>

class Hash;
class HashView : public View<32> {
public:
    inline HashView(const uint8_t* pos)
        : View<32>(pos) {};
    inline bool operator==(HashView hv) const
    {
        return View::operator==(hv);
    };
    inline bool operator!=(HashView hv) const
    {
        return !operator==(hv);
    }
};

class Hash : public std::array<uint8_t, 32> {
public:
    Hash() = default;
    Hash(const std::array<uint8_t, 32>& other)
        : array(other)
    {
    }
    Hash(const Hash&) = default;
    Hash(Hash&&) = default;
    operator HashView() const
    {
        return HashView(data());
    }
    Hash(HashView hv)
    {
        memcpy(data(), hv.data(), 32);
    }
    Hash& operator=(const Hash&) = default;
    static Hash genesis();
};
class TxHash : public Hash {
public:
    explicit TxHash(Hash h)
        : Hash(h)
    {
    }
};
