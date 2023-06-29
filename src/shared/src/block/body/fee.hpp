#pragma once
#include "general/funds.hpp"
#include <cstdint>
class Writer;
Writer& operator<<(Writer&, CompactFee);
class CompactFee {
public:
    CompactFee(uint16_t val)
        : val(val)
    {
    }
    static CompactFee compact(Funds);
    operator Funds() const
    {
        return uncompact();
    }
    Funds uncompact() const
    { // OK
        uint64_t e = (val & uint64_t(0xFC00u)) >> 10;
        uint64_t m = (val & uint64_t(0x03FFu)) + uint64_t(0x0400u);
        if (e < 10) {
            return Funds(m >> (10 - e));
        } else {
            return Funds(m << (e - 10));
        }
    }
    uint16_t value() const { return val; }

    // default comparison is correct even without uncompacting.
    auto operator<=>(const CompactFee&) const = default;

private:
    uint16_t val;
};
