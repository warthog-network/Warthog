#pragma once
#include "general/errors.hpp"
#include "general/with_uint64.hpp"
#include <cassert>
#include <optional>

class Writer;
class CompactUInt;

class Funds_uint64 : public IsUint64 {
public:
    constexpr Funds_uint64(uint64_t val)
        : IsUint64(val) { };
    static constexpr Funds_uint64 zero() { return { 0 }; }
    auto operator<=>(const Funds_uint64&) const = default;

    static std::optional<Funds_uint64> parse(std::string_view);
    static Funds_uint64 parse_throw(std::string_view);
    bool is_zero() const { return val == 0; }
    // std::string format(std::string_view unit) const;
    std::string to_string() const;

    void add_throw(Funds_uint64 add)
    {
        *this = sum_throw(*this, add);
    }
    void add_assert(Funds_uint64 add)
    {
        *this = sum_assert(*this, add);
    }

    static std::optional<Funds_uint64> sum(Funds_uint64 a, Funds_uint64 b)
    {
        auto s { a.val + b.val };
        if (s < a.val)
            return {};
        return Funds_uint64 { s };
    }

    template <typename... T>
    static std::optional<Funds_uint64> sum(Funds_uint64 a, Funds_uint64 b, T&&... t)
    {
        auto s { sum(a, b) };
        if (!s.has_value())
            return {};
        return sum(*s, std::forward<T>(t)...);
    }

    template <typename... T>
    static Funds_uint64 sum_throw(Funds_uint64 a, T&&... t)
    {
        auto s { sum(a, std::forward<T>(t)...) };
        if (!s.has_value())
            throw Error(EBALANCE);
        return *s;
    }

    template <typename... T>
    static Funds_uint64 sum_assert(Funds_uint64 a, T&&... t)
    {
        auto s { sum(a, std::forward<T>(t)...) };
        assert(s.has_value());
        return *s;
    }

    void subtract_assert(Funds_uint64 f)
    {
        *this = diff_assert(*this, f);
    }
    static std::optional<Funds_uint64> diff(Funds_uint64 a, Funds_uint64 b)
    {
        if (a.val < b.val)
            return {};
        return Funds_uint64 { a.val - b.val };
    }
    static Funds_uint64 diff_assert(Funds_uint64 a, Funds_uint64 b)
    {
        auto d { diff(a, b) };
        assert(d.has_value());
        return *d;
    }
    static Funds_uint64 diff_throw(Funds_uint64 a, Funds_uint64 b)
    {
        auto d { diff(a, b) };
        if (!d.has_value())
            throw Error(EBALANCE);
        return *d;
    }
};

class Funds : public IsUint64 {
private:
    constexpr Funds(uint64_t val)
        : IsUint64(val) { };

public:
    static constexpr Funds zero() { return { 0 }; }
    auto operator<=>(const Funds&) const = default;
    static constexpr std::optional<Funds> from_value(uint64_t value)
    {
        return Funds(value);
    }
    static constexpr Funds from_value_throw(uint64_t value)
    {
        auto v { from_value(value) };
        if (!v)
            throw Error(EINV_FUNDS);
        return *v;
    }

    static std::optional<Funds> parse(std::string_view);
    static Funds parse_throw(std::string_view);
    bool is_zero() const { return val == 0; }
    // std::string format(std::string_view unit) const;
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
