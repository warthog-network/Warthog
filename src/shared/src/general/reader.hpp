#pragma once

#include "block/chain/worksum.hpp"
#include "general/errors.hpp"
#include "view.hpp"
#include "general/byte_order.hpp"
#include <span>

// inline funcitons for access
inline uint64_t readuint64(const uint8_t* pos)
{
    uint64_t val;
    memcpy(&val, pos, 8);
    return ntoh64(val);
}
inline uint32_t readuint32(const uint8_t* pos)
{
    uint32_t res;
    memcpy(&res, pos, 4);
    return ntoh32(res);
}
inline uint16_t readuint16(const uint8_t* pos)
{
    uint16_t res;
    memcpy(&res, pos, 2);
    return ntoh16(res);
}
inline uint8_t readuint8(const uint8_t* pos) { return *(pos); }

template <size_t N>
class ReaderCheck;
// byte sequence stream-like reader with self-advancing cursor
class Reader {
    inline void read(void* out, size_t bytes)
    {
        auto newpos { pos + bytes };
        if (newpos > end)
            throw Error(EMALFORMED);
        memcpy(out, pos, bytes);
        pos = newpos;
    }
    template <typename T>
    T read()
    {
        T t;
        read(&t, sizeof(T));
        return t;
    }

public:
    Reader(std::span<const uint8_t> s)
        : pos(s.data())
        , end(s.data() + s.size())
    {
    }
    template <size_t N>
    operator std::array<uint8_t, N>()
    {
        return view<N>();
    }
    uint64_t uint64()
    {
        static_assert(sizeof(uint64_t) == 8);
        return ntoh64(read<uint64_t>());
    }
    uint32_t uint32()
    {
        static_assert(sizeof(uint32_t) == 4);
        return ntoh32(read<uint32_t>());
    }
    uint16_t uint16()
    {
        static_assert(sizeof(uint16_t) == 2);
        return ntoh16(read<uint16_t>());
    }
    operator uint16_t()
    {
        return uint16();
    }
    operator uint32_t()
    {
        return uint32();
    }
    uint8_t uint8()
    {
        return read<uint8_t>();
    }
    template <size_t N>
    View<N> view()
    {
        View<N> v(pos);
        skip(N);
        return v;
    }
    template <size_t N>
    operator View<N>()
    {
        return view<N>();
    }
    template <typename T, size_t N = T::size()>
    requires std::derived_from<T, View<N>>
    [[nodiscard]] T view()
    {
        T t(pos);
        skip(N);
        return { t };
    }
    void copy_checkrange(void* dst, size_t len)
    {
        read(dst, len);
    }
    Worksum worksum()
    {
        Worksum::fragments_type arr;
        for (size_t i = 0; i < arr.size(); ++i) {
            arr[i] = uint32();
        }
        return Worksum(arr);
    }
    std::span<const uint8_t> take_span(size_t len){
        auto p = pos;
        skip(len);
        return { p, len };
    }
    std::span<const uint8_t> span()
    {
        return take_span(uint32());
    }
    std::span<const uint8_t> rest(){
        return take_span(remaining());
    };

    void seek(const uint8_t* cursor) { pos = cursor; }
    void skip(size_t nbytes)
    {
        pos += nbytes;
        if (pos > end) {
            throw Error(EMALFORMED);
        }
    };
    bool eof() const { return pos == end; }
    void consumed() { pos = end; }
    const uint8_t* cursor() const { return pos; }
    size_t remaining() const { return end - pos; }

private:
    const uint8_t* pos;
    const uint8_t* end;
};

template <size_t N>
class ReaderCheck {
public:
    ReaderCheck(Reader& r)
        : r(r)
        , beginPos(r.cursor())
    {
    }
    operator Reader&()
    {
        return r;
    }
    template <size_t M>
    operator ReaderCheck<M>()
    {
        return { r };
    }
    ~ReaderCheck()
    {
        assert(r.cursor() == beginPos + N);
    }
    Reader& r;
    const uint8_t* const beginPos;
};
