#pragma once
#include <cstddef>
#include <string>

template <size_t N>
struct StaticString {
    constexpr StaticString(const char (&str)[N])
    {
        for (size_t i { 0 }; i < N; ++i)
            value[i] = str[i];
    }
    std::string to_string() const
    {
        return std::string { value, N };
    }

    char value[N];
};
