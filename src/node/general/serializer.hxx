#pragma once

#include "general/byte_order.hpp"
#include <cstdint>
#include <span>
#include <string_view>
template <typename T>
concept Serializer = requires(T t, const std::span<const uint8_t>& s) {
    { t.write(s) };
};

auto&& operator<<(Serializer auto&& s, std::span<const uint8_t> sp)
{
    s.write(sp);
    return std::forward<decltype(s)>(s);
}

auto&& operator<<(Serializer auto&& s, std::unsigned_integral auto v)
{
    auto valBe { byte_swap(v) };
    std::span<const uint8_t> sp((uint8_t*)&valBe, sizeof(v));
    return std::forward<decltype(s)>(s << sp);
}

auto&& operator<<(Serializer auto&& s, std::string_view r)
{
    std::span sp(reinterpret_cast<const uint8_t*>(r.data()),r.size());
    return std::forward<decltype(s)>(s<<sp);
}

