#pragma once

#include <cstddef>
template <size_t N>
struct FixedString {
    constexpr FixedString(const char (&str)[N])
    {
        for (size_t i = 0; i < N; ++i)
            value[i] = str[i];
    }
    char value[N];
};
