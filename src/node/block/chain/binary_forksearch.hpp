#pragma once
#include <cstddef>
#include <tuple>

template <typename T1, typename T2>
[[nodiscard]] inline size_t binary_forksearch(const T1& v1, const T2& v2, size_t lower, size_t upper)
{
    // Verified, OK.
    while (upper > lower) {
        size_t pos = lower + (upper-lower) / 2;
        if (v1[pos] == v2[pos])
            lower = pos + 1;
        else
            upper = pos;
    }
    return upper;
}

template <typename T1, typename T2>
[[nodiscard]] inline std::pair<size_t,bool> binary_forksearch(const T1& v1, const T2& v2, size_t lower = 0)
{
    const size_t s1 = v1.size();
    const size_t s2 = v2.size();
    const size_t len = (s1 > s2 ? s2 : s1);
    size_t res = binary_forksearch(v1, v2, lower, len);
    return {res,res != len};
}
