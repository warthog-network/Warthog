#pragma once

#include <cstdint>
#include <span>
template <typename T>
concept Serializer = requires(T t, const std::span<const uint8_t>& s) {
    { t.write(s) };
};
