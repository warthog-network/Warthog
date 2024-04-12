#pragma once
#include "general/view.hpp"
#include <array>

class Hash;
class HashView : public View<32> {
public:
    inline HashView(const uint8_t* pos)
        : View<32>(pos) {};
    inline bool operator==(HashView hv) const
    {
        return View::operator==(hv);
    };
    struct HashComparator {
        using arr = std::array<uint8_t, 32>;
        using is_transparent = std::true_type;
        bool operator()(const arr& arr1, const arr& arr2) const { return arr1 < arr2; }
        bool operator()(const arr& arr, HashView hv) const
        {
            return memcmp(arr.data(), hv.data(), 32) < 0;
        }
        bool operator()(HashView hv, const arr& arr) const
        {
            return memcmp(hv.data(), arr.data(), 32) < 0;
        }
    };
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
    bool operator==(const Hash&) const = default;
    bool operator!=(const Hash&) const = default;
    static Hash genesis();
};

inline bool operator==(const Hash& h, const HashView& hv)
{
    return  (HashView(h) == hv);
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
