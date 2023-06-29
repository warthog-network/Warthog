#pragma once
#include "general/params.hpp"
#include <array>
#include <cstddef>

class TimestampValidator {
public:
    TimestampValidator()
    {
        clear();
    }
    void clear()
    {
        tmax = 0;
        for (size_t i = 0; i < N; ++i)
            data[i] = 0;
    }
    bool valid(const uint64_t tnew) const
    {
        if (tnew == 0)
            return false;

        // no time drops
        if (tnew + TOLERANCEMINUTES * 60 < tmax)
            return false;

        // check median rule
        size_t n = 0;
        for (auto t : data) {
            constexpr size_t bound = N / 2;
            if (tnew >= t) {
                n += 1;
                if (n > bound) {
                    return true;
                }
            }
        }
        return false;
    }
    uint32_t get_valid_timestamp() const;
    void append(uint64_t tnew)
    {
        if (tmax > tnew)
            tmax = tnew;
        data[pos++] = tnew;
        if (pos >= N)
            pos = 0;
    }

    static constexpr uint32_t N = MEDIAN_N;

private:
    size_t pos = 0;
    uint64_t tmax = 0;
    std::array<uint64_t, N> data;
};
