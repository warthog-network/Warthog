#pragma once
#include <optional>
#include <vector>

template <typename T>
size_t byte_size(T&& t)
{
    using TT = std::remove_cvref_t<T>;
    if constexpr (std::is_standard_layout_v<TT> && std::is_trivial_v<TT>) {
        return sizeof(t);
    } else {
        return t.byte_size();
    }
}

template <typename T>
size_t byte_size(const std::vector<T>& v)
{
    size_t i = 0;
    for (auto& e : v) {
        i += ::byte_size(e);
    }
    return i;
}

template <typename T>
size_t byte_size(const std::optional<T>& t)
{
    if (t)
        return 1 + byte_size(*t);
    return 1;
}

template <typename T, typename U>
size_t byte_size(const std::pair<T, U>& p)
{
    return byte_size(p.first) + byte_size(p.second);
}
