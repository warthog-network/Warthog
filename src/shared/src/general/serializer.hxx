#pragma once

#include "general/byte_order.hpp"
#include "serializer_fwd.hxx"
#include <cstdint>
#include <span>
#include <string_view>

template <typename S, typename T>
concept Serializing = Serializer<S> && requires(S& s, const T& t) {
    { t.serialize(s) };
};

auto&& operator<<(Serializer auto&& s, std::span<const uint8_t> sp)
{
    s.write(sp);
    return std::forward<decltype(s)>(s);
}

auto&& operator<<(Serializer auto&& s, ByteSwappable auto v)
{
    auto valBe { network_byte_swap(v) };
    std::span<const uint8_t> sp((uint8_t*)&valBe, sizeof(v));
    return std::forward<decltype(s)>(s << sp);
}

auto&& operator<<(Serializer auto&& s, bool b)
{
    return std::forward<decltype(s)>(s << (b ? uint8_t(1) : uint8_t(0)));
}

auto&& operator<<(Serializer auto&& s, std::string_view r)
{
    std::span sp(reinterpret_cast<const uint8_t*>(r.data()), r.size());
    return std::forward<decltype(s)>(s << sp);
}

template <typename S, typename T>
requires Serializer<S> && Serializing<S, T>
auto&& operator<<(S&& s, const T& t)
{
    t.serialize(s);
    return std::forward<S>(s);
}
