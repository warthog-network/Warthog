#pragma once

#include "general/byte_order.hpp"
#include "general/view.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct Range {
    Range(std::string_view sv)
        : pos((uint8_t*)(&sv[0]))
        , len(sv.size())
    {
    }

    Range(const uint8_t* const pos, size_t len)
        : pos(pos)
        , len(len)
    {
    }
    Range(const std::vector<uint8_t>& bytes)
        : pos(bytes.data())
        , len(bytes.size())
    {
    }
    template <size_t N>
    Range(const std::array<uint8_t, N>& bytes)
        : pos(bytes.data())
        , len(bytes.size())
    {
    }
    template <size_t N>
    Range(View<N> v)
        : pos(v.data())
        , len(v.size())
    {
    }
    const uint8_t* const pos;
    size_t len;
};

class Writer {
    template <size_t... Is, typename... Ts>
    Writer& write_tuple(const std::tuple<Ts...>& t)
    {
        return (*this) << std::get<Is...>(t);
    }

public:
    Writer(uint8_t* pos, size_t n)
        : pos(pos)
        , end(pos + n)
    {
    }
    Writer(std::span<uint8_t> s)
        : Writer(s.data(), s.size())
    {
    }
    ~Writer() { assert(pos <= end); }
    Writer& operator<<(bool b)
    {
        return operator<<(uint8_t(b));
    }
    Writer& operator<<(uint8_t i)
    {
        assert(remaining() >= 1);
        *(pos++) = i;
        return *this;
    }
    Writer& operator<<(uint16_t i)
    {
        assert(remaining() >= 2);
        i = hton16(i);
        memcpy(pos, &i, 2);
        pos += 2;
        return *this;
    }

    Writer& operator<<(uint32_t i)
    {
        assert(remaining() >= 4);
        i = hton32(i);
        memcpy(pos, &i, 4);
        pos += 4;
        return *this;
    }

    template <typename T>
    Writer& operator<<(const std::optional<T>& o)
    {
        if (o)
            return *this << uint8_t(1) << *o;
        else
            return *this << uint8_t(0);
    }

    Writer& operator<<(uint64_t i)
    {
        assert(remaining() >= 8);
        i = hton64(i);
        memcpy(pos, &i, 8);
        pos += 8;
        return *this;
    }

    Writer& operator<<(const std::string& s)
    {
        return operator<<(std::string_view(s));
    }
    Writer& operator<<(std::string_view r)
    {
        return operator<<(Range(r));
    }

    template <typename... Ts>
    Writer& operator<<(const std::tuple<Ts...>& t)
    {
        return write_tuple<std::index_sequence_for<Ts...>, Ts...>(t);
    }

    Writer& operator<<(const Range& r)
    {
        assert(remaining() >= r.len);
        memcpy(pos, r.pos, r.len);
        pos += r.len;
        return *this;
    }

    uint8_t* cursor() { return pos; }
    void skip(size_t bytes) { pos += bytes; }
    size_t remaining()
    {
        assert(end >= pos);
        return end - pos;
    }

private:
    uint8_t* pos;
    uint8_t* const end;
};
