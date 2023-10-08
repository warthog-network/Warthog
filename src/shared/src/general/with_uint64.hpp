#pragma once
#include "general/reader_declaration.hpp"
#include "general/writer.hpp"
#include "nlohmann/json_fwd.hpp"
#include <cstdint>

struct IsUint32 {
public:
    IsUint32(Reader& r);
    explicit IsUint32(int64_t w);
    explicit IsUint32(int w)
        : IsUint32((int64_t)(w)) {};
    // explicit IsUint32(long long w)
    //     : IsUint32((int64_t)(w)) {};
    explicit IsUint32(size_t w);
    constexpr explicit IsUint32(uint32_t val)
        : val(val) {};

    bool operator==(const IsUint32&) const = default;
    auto operator<=>(const IsUint32&) const = default;
    friend Writer& operator<<(Writer& w, const IsUint32& v)
    {
        return w << v.val;
    }
    operator nlohmann::json() const;

    uint32_t value() const
    {
        return val;
    }

protected:
    uint32_t val;
};
struct IsUint64 {
public:
    explicit IsUint64(int64_t w)
        : val(w)
    {
        assert(w >= 0);
    };
    IsUint64(Reader& r);
    explicit IsUint64(int w)
        : IsUint64((int64_t)(w)) {};
    // explicit IsUint64(long long w)
    //     : IsUint64((int64_t)(w)) {};
    explicit IsUint64(uint64_t val)
        : val(val) {};
    // explicit IsUint64(unsigned long long val)
        // : val(val) {};

    bool operator==(const IsUint64&) const = default;
    auto operator<=>(const IsUint64&) const = default;
    friend Writer& operator<<(Writer& w, const IsUint64& v)
    {
        return w << v.val;
    }

    operator nlohmann::json() const;
    uint64_t value() const
    {
        return val;
    }

protected:
    uint64_t val;
};
