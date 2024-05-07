#pragma once
#include <cstddef>
#include <optional>
#include <type_traits>

template <typename T>
size_t byte_size(T&& t)
{
    if constexpr (std::is_pod_v<std::remove_cvref_t<T>>){
        return sizeof(t);
    }
    return t.byte_size();
}

template<typename T>
size_t byte_size(const std::optional<T>& t){
    if (t) 
        return 1+byte_size(*t);
    return 1;
}
