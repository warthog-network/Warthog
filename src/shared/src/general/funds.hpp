#pragma once
#include "general/params.hpp"
#include "general/with_uint64.hpp"
#include <optional>

class Writer;
class CompactUInt;
class Funds : public IsUint64 {
public:
    using IsUint64::IsUint64;
    static std::optional<Funds> parse(std::string_view);
    static Funds throw_parse(std::string_view);
    bool overflow()
    {
        return val > MAXSUPPLY;
    }
    Funds(std::string_view);
    bool is_zero() const { return val == 0; }
    void assert_bounds() const;
    std::string format() const;
    std::string to_string() const;
    uint64_t E8() const { return val; };
    Funds& operator+=(Funds f)
    {
        val += f.val;
        return *this;
    };
    Funds operator+(Funds f) const
    {
        return Funds(*this) += f;
    }
    Funds& operator-=(Funds f)
    {
        val -= f.val;
        return *this;
    }
    Funds operator-(Funds f) const
    {
        return Funds(*this) -= f;
    }

private:
    // we use the more meaningful E8 instead
    uint64_t value() const = delete;
};
