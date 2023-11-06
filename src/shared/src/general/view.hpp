#pragma once
#include <array>
#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstring>
template <size_t N>
struct View {
    static constexpr size_t size() { return N; }
    bool is_null() const { return pos == nullptr; }
    const uint8_t* data() const { return pos; }
    operator std::array<uint8_t, N>()
    {
        std::array<uint8_t, N> res;
        std::copy(pos, pos + N, res.begin());
        return res;
    }
    auto operator<=>(const View& v) const
    {
        assert(!is_null() && !v.is_null());
        auto i = memcmp(pos, v.pos, N);
        if (i < 0)
            return std::strong_ordering::less;
        else if (i > 0)
            return std::strong_ordering::greater;
        else
            return std::strong_ordering::equal;
    }
    bool operator==(const View& v) const
    {
        return operator<=>(v) == 0;
    }
    View(const std::array<uint8_t, N>& a)
        : View(a.data())
    {
    }
    explicit View(const uint8_t* pos)
        : pos(pos)
    {
    }

protected:
    const uint8_t* pos;
};
