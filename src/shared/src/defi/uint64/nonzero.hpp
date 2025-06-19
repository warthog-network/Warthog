#pragma once
#include <cassert>
#include <cstdint>

struct Nonzero_uint64 {
    constexpr explicit Nonzero_uint64(uint64_t v)
        : value_(v)
    {
        assert(v != 0);
    }
    uint64_t value() const { return value_; }
    operator uint64_t() const { return value(); }

private:
    uint64_t value_;
};
