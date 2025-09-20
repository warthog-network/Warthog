#pragma once
#include <cstddef>
#include <tuple>
template <typename T>
struct function_traits : function_traits<decltype(&std::remove_cvref_t<T>::operator())> { };

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...) const> {
    using result_type = ReturnType;
    template <size_t i>
    struct arg {
        using type = typename std::tuple_element<i, std::tuple<Args...>>::type;
    };
    static constexpr size_t arity = sizeof...(Args);
};
