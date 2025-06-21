#pragma once
#include "defi/token/id.hpp"
#include "general/errors.hpp"
#include "general/with_uint64.hpp"
#include <cassert>
#include <optional>

class AssetPrecision { // number of decimal places
private:
    uint8_t val;

    struct Token { };
    constexpr AssetPrecision(uint8_t v, Token)
        : val(v)
    {
    }

    static constexpr const uint8_t max { 18 };

public:
    static constexpr size_t byte_size() { return 1; }
    auto value() const { return val; }
    consteval AssetPrecision(size_t v)
        : AssetPrecision(uint8_t(v), Token())
    {
        if (v > max)
            throw std::runtime_error("Value " + std::to_string(v) + " exceeds maximum " + std::to_string(max) + ".");
    }
    AssetPrecision(Reader& r);
    friend Writer& operator<<(Writer& w, const AssetPrecision&);
    static const AssetPrecision zero;
    static constexpr AssetPrecision digits8()
    {
        return 8;
    }
    constexpr auto operator()() const { return val; }
    static constexpr AssetPrecision from_number_throw(uint8_t v)
    {
        if (auto o { from_number(v) })
            return *o;
        throw Error(ETOKENPRECISION);
    }
    static constexpr std::optional<AssetPrecision> from_number(uint8_t v)
    {
        if (v > max)
            return {};
        return AssetPrecision { v, Token() };
    }
};
constexpr const AssetPrecision AssetPrecision::zero { 0 };

struct ParsedFunds {
    [[nodiscard]] static std::optional<ParsedFunds> parse(std::string_view);
    ParsedFunds(std::string_view);
    ParsedFunds(uint64_t v, uint8_t decimalPlaces)
        : v(v)
        , decimalPlaces(decimalPlaces)
    {
    }
    std::string to_string() const;
    auto uint64() const { return v; };
    uint64_t v;
    uint8_t decimalPlaces;
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

    static std::optional<R> sum(FundsBase<R> a, FundsBase<R> b)
    {
        auto s { a.val + b.val };
        if (s < a.val)
            return {};
        return from_value(s);
    }

    template <typename... T>
    static std::optional<R> sum(FundsBase<R> a, FundsBase<R> b, T&&... t)
    {
        auto s { sum(a, b) };
        if (!s.has_value())
            return {};
        return sum(*s, std::forward<T>(t)...);
    }

    template <typename... T>
    static R sum_throw(FundsBase<R> a, T&&... t)
    {
        auto s { sum(a, std::forward<T>(t)...) };
        if (!s.has_value())
            throw Error(EBALANCE);
        return *s;
    }

    template <typename... T>
    static R sum_assert(FundsBase<R> a, T&&... t)
    {
        auto s { sum(a, std::forward<T>(t)...) };
        assert(s.has_value());
        return *s;
    }

    void subtract_assert(FundsBase<R> f)
    {
        *this = diff_assert(*this, f);
    }
    static std::optional<R> diff(FundsBase<R> a, FundsBase<R> b)
    {
        if (a.val < b.val)
            return {};
        return from_value(a.val - b.val);
    }
    static R diff_assert(FundsBase<R> a, FundsBase<R> b)
    {
        auto d { diff(a, b) };
        assert(d.has_value());
        return *d;
    }
    static R diff_throw(FundsBase<R> a, FundsBase<R> b)
    {
        auto d { diff(a, b) };
        if (!d.has_value())
            throw Error(EBALANCE);
        return *d;
    }
};

struct FundsDecimal;
class Funds_uint64 : public FundsBase<Funds_uint64> {
public:
    constexpr Funds_uint64(uint64_t v)
        : FundsBase<Funds_uint64>(from_value_throw(v))
    {
    }
    Funds_uint64(Reader& r);
    auto operator<=>(const Funds_uint64&) const = default;
    static std::optional<Funds_uint64> parse(std::string_view, AssetPrecision);
    static std::optional<Funds_uint64> parse(ParsedFunds, AssetPrecision);
    static Funds_uint64 parse_throw(std::string_view, AssetPrecision);
    FundsDecimal to_decimal(AssetPrecision d) const;
};
class NonzeroFunds_uint64 : public Funds_uint64 {
public:
    constexpr NonzeroFunds_uint64(Funds_uint64 f)
        : Funds_uint64(f)
    {
        assert(f != 0);
    }
    auto operator<=>(const NonzeroFunds_uint64&) const = default;
};
struct FundsDecimal {
    Funds_uint64 funds;
    AssetPrecision precision;
    constexpr static size_t byte_size() { return Funds_uint64::byte_size() + AssetPrecision::byte_size(); }
    friend Writer& operator<<(Writer& w, const FundsDecimal& fd);
    FundsDecimal(Reader& r)
        : FundsDecimal(r, r)
    {
    }
    FundsDecimal(Funds_uint64 funds, AssetPrecision precision)
        : funds(std::move(funds))
        , precision(std::move(precision))
    {
    }
    static FundsDecimal zero() { return { 0, 0 }; }
    std::string to_string() const;
};

inline FundsDecimal Funds_uint64::to_decimal(AssetPrecision d) const
{
    return { value(), d };
}


class Wart : public FundsBase<Wart> {
public:
    static constexpr AssetPrecision precision { AssetPrecision::digits8() };
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
    static std::optional<Wart> parse(ParsedFunds);
    static Wart parse_throw(std::string_view);
    std::string to_string() const;
    uint64_t E8() const { return val; };
    operator Funds_uint64() const
    {
        return Funds_uint64::from_value_throw(E8());
    }

private:
    // we use the more meaningful E8 instead
    uint64_t value() const = delete;
};
