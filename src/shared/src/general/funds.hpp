#pragma once
#include "defi/token/id.hpp"
#include "general/errors.hpp"
#include "general/with_uint64.hpp"
#include <cassert>
#include <optional>

class DecimalDigits {
private:
    uint8_t val;

    constexpr DecimalDigits(uint8_t v)
        : val(v)
    {
    }

public:
    static constexpr const uint8_t max { 18 };

    static constexpr DecimalDigits digits8()
    {
        return 8;
    }
    constexpr auto operator()() const { return val; }
    constexpr std::optional<DecimalDigits> from_number(uint8_t v)
    {
        if (v > max)
            return {};
        return DecimalDigits { v };
    }
};

struct FundsDecimal {
    [[nodiscard]] static std::optional<FundsDecimal> parse(std::string_view);
    FundsDecimal(std::string_view);
    FundsDecimal(uint64_t v, uint8_t decimals)
        : v(v)
        , decimals(decimals)
    {
    }
    static FundsDecimal zero() { return { 0, 0 }; }
    std::string to_string() const;
    uint64_t v;
    uint8_t decimals;
};

template <typename R>
class FundsBase : public IsUint64 {
private:
    constexpr FundsBase(uint64_t val)
        : IsUint64(val) { };

public:
    static constexpr R zero() { return { 0 }; }
    auto operator<=>(const FundsBase<R>&) const = default;
    static constexpr std::optional<R> from_value(uint64_t value)
    {
        return R(value);
    }
    static constexpr R from_value_throw(uint64_t value)
    {
        auto v { from_value(value) };
        if (!v)
            throw Error(EINV_FUNDS);
        return *v;
    }

    bool is_zero() const { return val == 0; }
    // std::string format(std::string_view unit) const;

    void add_throw(R add)
    {
        *this = sum_throw(*this, add);
    }
    void add_assert(R add)
    {
        *this = sum_assert(*this, add);
    }

    static std::optional<R> sum(R a, R b)
    {
        auto s { a.val + b.val };
        if (s < a.val)
            return {};
        return from_value(s);
    }

    template <typename... T>
    static std::optional<R> sum(R a, R b, T&&... t)
    {
        auto s { sum(a, b) };
        if (!s.has_value())
            return {};
        return sum(*s, std::forward<T>(t)...);
    }

    template <typename... T>
    static R sum_throw(R a, T&&... t)
    {
        auto s { sum(a, std::forward<T>(t)...) };
        if (!s.has_value())
            throw Error(EBALANCE);
        return *s;
    }

    template <typename... T>
    static R sum_assert(R a, T&&... t)
    {
        auto s { sum(a, std::forward<T>(t)...) };
        assert(s.has_value());
        return *s;
    }

    void subtract_assert(R f)
    {
        *this = diff_assert(*this, f);
    }
    static std::optional<R> diff(R a, R b)
    {
        if (a.val < b.val)
            return {};
        return from_value(a.val - b.val);
    }
    static R diff_assert(R a, R b)
    {
        auto d { diff(a, b) };
        assert(d.has_value());
        return *d;
    }
    static R diff_throw(R a, R b)
    {
        auto d { diff(a, b) };
        if (!d.has_value())
            throw Error(EBALANCE);
        return *d;
    }
};

class Funds_uint64 : public FundsBase<Funds_uint64> {
public:
    constexpr Funds_uint64(uint64_t v)
        : FundsBase<Funds_uint64>(from_value_throw(v))
    {
    }
    Funds_uint64(Reader& r);
    auto operator<=>(const Funds_uint64&) const = default;
    static std::optional<Funds_uint64> parse(std::string_view, DecimalDigits);
    static std::optional<Funds_uint64> parse(FundsDecimal, DecimalDigits);
    static Funds_uint64 parse_throw(std::string_view, DecimalDigits);
    FundsDecimal to_decimal(DecimalDigits d) const
    {
        return { value(), d() };
    }
    std::string to_string(DecimalDigits) const;
};

struct TokenFunds {
    TokenId tokenId;
    Funds_uint64 funds;
};

class Wart : public FundsBase<Wart> {
public:
    constexpr Wart(uint64_t v)
        : FundsBase<Wart>(from_value_throw(v))
    {
    }
    Wart(Reader& r);
    static Wart from_funds_throw(Funds_uint64 f)
    {
        return from_value_throw(f.value());
    }
    auto operator<=>(const Wart&) const = default;
    static std::optional<Wart> parse(std::string_view);
    static std::optional<Wart> parse(FundsDecimal);
    static Wart parse_throw(std::string_view);
    std::string to_string() const;
    uint64_t E8() const { return val; };
    operator Funds_uint64() const
    {
        return Funds_uint64::from_value_throw(E8());
    }
    operator TokenFunds() const
    {
        return { .tokenId { TokenId(0) }, .funds { *this } };
    }

private:
    // we use the more meaningful E8 instead
    uint64_t value() const = delete;
};
