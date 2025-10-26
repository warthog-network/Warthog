#pragma once

#include "general/byte_order.hpp"
#include "serializer_fwd.hxx"
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

template <typename S, typename T>
concept Serializing = Serializer<S> && requires(S& s, const T& t) {
    { t.serialize(s) };
};

constexpr auto&& operator<<(Serializer auto&& s, std::span<const uint8_t> sp)
{
    s.write(sp);
    return std::forward<decltype(s)>(s);
}

constexpr auto&& operator<<(Serializer auto&& s, ByteSwappable auto v)
{
    auto valBe { network_byte_swap(v) };
    std::span<const uint8_t> sp((uint8_t*)&valBe, sizeof(v));
    return std::forward<decltype(s)>(s << sp);
}

template <typename T>
auto&& operator<<(Serializer auto&& s, const std::optional<T>& o)
{
    if (o)
        return std::forward<decltype(s)>(s << uint8_t(1) << *o);
    else
        return std::forward<decltype(s)>(s << uint8_t(0));
}

constexpr auto&& operator<<(Serializer auto&& s, bool b)
{
    return std::forward<decltype(s)>(s << (b ? uint8_t(1) : uint8_t(0)));
}

constexpr auto&& operator<<(Serializer auto&& s, std::string_view r)
{
    std::span sp(reinterpret_cast<const uint8_t*>(r.data()), r.size());
    return std::forward<decltype(s)>(s << sp);
}

template <typename S, typename T>
requires Serializer<S> && Serializing<S, T>
constexpr auto&& operator<<(S&& s, const T& t)
{
    t.serialize(s);
    return std::forward<S>(s);
}

struct ByteCounter {
    size_t N { 0 };
    constexpr void write(std::span<const uint8_t> s)
    {
        N += s.size();
    }
};
template <typename T>
constexpr size_t count_bytes(const T& t)
{
    return (ByteCounter() << t).N;
}
