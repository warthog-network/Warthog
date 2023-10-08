#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include "general/view.hpp"
#include "general/byte_order.hpp"

struct Range {
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
public:
    Writer(std::vector<uint8_t>& v)
        : Writer(v.data(), v.data() + v.size()) {};
    Writer(uint8_t* pos, size_t n)
        : Writer(pos, pos + n) {};
    Writer(uint8_t* pos, uint8_t* end)
        : pos(pos)
        , end(end)
    {
    }
    ~Writer() { assert(pos <= end); }
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

    Writer& operator<<(uint64_t i)
    {
        assert(remaining() >= 8);
        i = hton64(i);
        memcpy(pos, &i, 8);
        pos += 8;
        return *this;
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
