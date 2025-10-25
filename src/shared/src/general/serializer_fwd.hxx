#pragma once
#include <cstdint>
#include <span>

template <typename T>
concept Serializer = requires(T t, const std::span<const uint8_t>& s) {
    { t.write(s) };
};

template <typename R>
concept IsReader = std::is_same_v<typename R::is_reader, std::true_type>;
