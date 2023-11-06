#pragma once
#include <array>
#include <limits>
#include <string>
class TargetV1;
class TargetV2;
class Target;
class Worksum {
private:
    static constexpr size_t BITS = 256;
    static constexpr size_t ELEMENTS = BITS / (8 * 4 /*CHAR_BIT*sizeof(uint32_t)*/);

public:
    using fragments_type = std::array<uint32_t, ELEMENTS>;

private:
    fragments_type fragments;

public:
    const std::array<uint32_t, BITS / (8 * 4 /*CHAR_BIT*sizeof(uint32_t)*/)> getFragments() const { return fragments; }
    static size_t bytesize() { return sizeof(uint32_t) * ELEMENTS; };
    static Worksum max()
    {
        Worksum ws;
        for (auto& f : ws.fragments)
            f = std::numeric_limits<uint32_t>::max();
        return ws;
    }
    bool operator==(const Worksum& w) const = default;
    std::string to_string() const;
    Worksum& operator-=(const Worksum& w);
    Worksum& operator*=(uint32_t factor);
    Worksum& operator+=(const Worksum& w);
//     Worksum& operator+=(const Target& t);
// Worksum& Worksum::operator+=(const Target& t)
// {
//     return this->operator+=(Worksum(t));
// }
    friend Worksum operator+(Worksum w1, const Worksum& w2)
    {
        return w1 += w2;
    }
    // friend Worksum operator+(Worksum w, const TargetV1& t)
    // {
    //     return w += t;
    // }
    inline bool operator<(const Worksum& rhs) const
    {
        size_t j = fragments.size();
        while (j != 0) {
            j -= 1;
            if (fragments[j] != rhs.fragments[j])
                return (fragments[j] < rhs.fragments[j]);
        }
        return false;
    }
    inline bool operator>(const Worksum& rhs) const
    {
        return rhs < *this;
    }
    inline bool operator<=(const Worksum& rhs) const
    {
        return !operator>(rhs);
    }
    inline bool operator>=(const Worksum& rhs) const
    {
        return operator>(rhs) || operator==(rhs);
    }
    inline bool is_zero()
    {
        return *this == Worksum();
    }
    inline void setzero()
    {
        for (auto& f : fragments) {
            f = 0;
        }
    }

    double getdouble() const
    {
        double factor = 1.0;
        double sum = double(fragments[0]);
        for (size_t i = 1; i < fragments.size(); ++i) {
            factor *= 4294967296.0;
            sum += factor * double(fragments[i]);
        }
        return sum;
    }
    std::array<uint8_t, BITS / 8> to_bytes() const;
    Worksum()
    {
        fragments.fill(0ul);
    }
    Worksum(fragments_type fragments)
        : fragments(fragments)
    {
    }
    Worksum(std::array<uint8_t, 32> data);
    Worksum(const TargetV1& t);
    Worksum(const TargetV2& t);
    Worksum(const Target& t);
};
