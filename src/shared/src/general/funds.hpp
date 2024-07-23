#pragma once
#include "general/errors.hpp"
#include "general/params.hpp"
#include "general/with_uint64.hpp"
#include <optional>

class Writer;
class CompactUInt;

class Funds : public IsUint64 {
private:
    constexpr Funds(uint64_t val)
        : IsUint64(val) {};

public:
    static constexpr Funds zero() { return { 0 }; }
    auto operator<=>(const Funds&) const = default;
    static std::optional<Funds> from_value(uint64_t value)
    {
        if (value > MAXSUPPLY)
            return {};
        return Funds(value);
    }
    static Funds from_value_throw(uint64_t value)
    {
        auto v{from_value(value)};
        if (!v) 
            throw Error(EINV_FUNDS);
        return *v;
    }

    static std::optional<Funds> parse(std::string_view);
    static Funds parse_throw(std::string_view);
    bool is_zero() const { return val == 0; }
    std::string format() const;
    std::string to_string() const;
    uint64_t E8() const { return val; };

    void add_throw(Funds add)
    {
        *this = sum_throw(*this, add);
    }
    void add_assert(Funds add)
    {
        *this = sum_assert(*this, add);
    }

    static std::optional<Funds> sum(Funds a, Funds b)
    {
        auto s { a.val + b.val };
        if (s < a.val)
            return {};
        return from_value(s);
    }

    template <typename... T>
    static std::optional<Funds> sum(Funds a, Funds b, T&&... t)
    {
        auto s { sum(a, b) };
        if (!s.has_value())
            return {};
        return sum(*s, std::forward<T>(t)...);
    }

    template <typename... T>
    static Funds sum_throw(Funds a, T&&... t)
    {
        auto s { sum(a, std::forward<T>(t)...) };
        if (!s.has_value())
            throw Error(EBALANCE);
        return *s;
    }

    template <typename... T>
    static Funds sum_assert(Funds a, T&&... t)
    {
        auto s { sum(a, std::forward<T>(t)...) };
        assert(s.has_value());
        return *s;
    }

    void subtract_assert(Funds f)
    {
        *this = diff_assert(*this, f);
    }
    static std::optional<Funds> diff(Funds a, Funds b)
    {
        if (a.val < b.val)
            return {};
        return Funds::from_value(a.val - b.val);
    }
    static Funds diff_assert(Funds a, Funds b)
    {
        auto d { diff(a, b) };
        assert(d.has_value());
        return *d;
    }
    static Funds diff_throw(Funds a, Funds b)
    {
        auto d { diff(a, b) };
        if (!d.has_value())
            throw Error(EBALANCE);
        return *d;
    }

private:
    // we use the more meaningful E8 instead
    uint64_t value() const = delete;
};
