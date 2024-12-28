#pragma once
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include "prod.hpp"
struct Price {
private:
  Price(uint16_t m, uint8_t e) : _e(e), _m(m) {}

  [[nodiscard]] static std::optional<Price> compose(auto mantissa, int e) {
    if (!is_exponent(e) || !is_mantissa(mantissa))
      return {};
    return Price(mantissa, e);
  }

public:
  [[nodiscard]] static Price zero() { return Price{0, 0}; }
  [[nodiscard]] static Price max() { return Price{0xFFFFu, 127}; }
  [[nodiscard]] static bool is_mantissa(uint32_t m) {
    return m <= mantissaMask && ((m >> (mantissaPrecision - 1)) != 0);
  }
  [[nodiscard]] static bool is_exponent(auto e) { return e >= 0 && e < 128; }
  static constexpr auto mantissaPrecision{16};
  static constexpr auto mantissaMask{(uint32_t(1) << mantissaPrecision) - 1};
  uint16_t mantissa() const { return _m; }
  auto exponent() const { return _e - 63; }
  auto mantissa_exponent() const { return exponent() - 16; }
  double to_double() const {
    auto a{mantissa()};
    auto b(mantissa_exponent());
    return std::ldexp(a, b);
  }
  [[nodiscard]] static std::optional<Price>
  from_mantissa_exponent(uint32_t mantissa, int exponent) {
    exponent += 63;
    return compose(mantissa, exponent);
  }

  std::optional<Price> prev_step() const {
    auto m{_m - 1};
    if (is_mantissa(m))
      return Price(m, _e);
    m = (_m << 1) - 1;
    assert(is_mantissa(m));
    auto e{_e - 1};
    if (!is_exponent(e))
      return {};
    return Price(m, e);
  }
  std::optional<Price> next_step() const {
    auto m{_m + 1};
    if (is_mantissa(m))
      return Price(m, _e);
    m >>= 1;
    auto e{_e + 1};
    if (!is_exponent(e)) // cannot represent with 8 bits exponent
      return {};
    return Price(m, e);
  }

  auto operator<=>(const Price &) const = default;

  static std::optional<Price> from_double(double d) {
    if (d <= 0.0)
      return {};
    int exp;
    double mantissa{std::frexp(d, &exp)};
    uint32_t mantissa32(mantissa * (1 << 16));
    return from_mantissa_exponent(mantissa32, exp);
  }

  static std::optional<Price> from_string(std::string s){
      try {
          return from_double(std::stod(s));
      }catch(...) {
          return {};
      }
  }
  

private:
  uint8_t _e;  // exponent
  uint16_t _m; // mantissa
};
struct PriceRelative { // gives details relative to price grid
  PriceRelative(Price price, bool exact = true)
      : price(std::move(price)), exact(std::move(exact)) {}
  auto operator<=>(Price p2) const {
    if (!exact && price == p2)
      return std::strong_ordering::greater;
    return price.operator<=>(p2);
  }
  const Price &floor() const { return price; }
  std::optional<Price> ceil() const {
    if (exact) {
      return price;
    }
    return price.next_step();
  }
  auto &operator=(Price p) { return *this = PriceRelative{p}; }
  auto operator<=>(PriceRelative p2) const {
    auto rel{price.operator<=>(p2.price)};
    if (rel == std::strong_ordering::equal) {
      if (exact && !p2.exact)
        return std::strong_ordering::less;
      if (!exact && p2.exact)
        return std::strong_ordering::greater;
    }
    return rel;
  }
  [[nodiscard]] static PriceRelative from_fraction(uint64_t numerator,
                                                   uint64_t denominator) { // OK
    if (numerator == 0) {
      assert(denominator !=
             0); // TODO: ensure this, i.e. numerator !=0 || denominator != 0
      return PriceRelative{Price::zero(), true};
    } else if (denominator == 0)
      return PriceRelative{Price::max(), false};

    int e{0};
    { // shift numerator
      auto z{std::countl_zero(numerator)};
      e -= z;
      numerator <<= z;
    }
    { // shift denominator
      auto z{std::countl_zero(denominator)};
      denominator <<= z;
      e += z;
    }

    constexpr uint64_t shiftr{(Price::mantissaPrecision)};
    constexpr uint64_t mask{(1 << shiftr) - 1};
    auto denominatorRest = denominator & mask;
    denominator >>= shiftr;

    uint64_t d{numerator / denominator};
    const auto rest{numerator - d * denominator};
    const auto dr{d * denominatorRest};
    bool exact{(dr & mask) == 0};
    const auto subtract{dr >> shiftr};
    if (rest < subtract || (rest == subtract && !exact)) {
      d -= 1;
    }
    if (rest != subtract)
      exact = false;
    auto r{64 - std::countl_zero(d)};
    if (r != Price::mantissaPrecision) {
      if ((d & 1) != 0)
        exact = false;
      d >>= 1;
      e += 1;
      r -= 1;
    }
    assert(r == Price::mantissaPrecision);

    auto p{Price::from_mantissa_exponent(d, e)};
    assert(p);
    return PriceRelative{*p, exact};
  }
  bool operator==(Price p2) const { return exact && price == p2; }
  Price price;
  bool exact;
};
inline std::optional<uint64_t> divide(uint64_t a, Price p, bool ceil) { // OK
  if (a == 0)
    return 0ull;
  auto z1{std::countl_zero(a)};
  a <<= z1;
  uint64_t d{a / p.mantissa()};
  assert(d != 0);
  uint64_t prod{(d * p.mantissa())};
  const auto z2{std::countl_zero(d)};
  uint64_t rest{(a - prod) << z2};
  auto d2{rest / p.mantissa()};
  bool inexact = d2 * p.mantissa() != rest;
  d = (d << z2) + d2;
  auto shift{-(p.mantissa_exponent() + z1 + z2)};
  if (shift > 0) // overflow
    return {};
  shift = -shift;
  if (shift >= 64) {
    if (d != 0)
      inexact = true;
    return 0 + (ceil && inexact);
  }
  if ((d << (64 - shift)) != 0) {
    inexact = true;
  }
  auto res{(d >> shift) + (ceil && inexact)};
  if (res == 0) // overflow
    return {};
  return res;
}

[[nodiscard]] inline std::optional<uint64_t> divide_floor(uint64_t a, Price p) {
  return divide(a, p, false);
}
[[nodiscard]] inline std::optional<uint64_t> divide_ceil(uint64_t a, Price p) {
  return divide(a, p, true);
}
inline std::optional<uint64_t> multiply_floor(uint64_t a, Price p) {
  return Prod128(p.mantissa(), a).pow2_64(p.mantissa_exponent(), false);
}

inline std::optional<uint64_t> multiply_ceil(uint64_t a, Price p) {
  return Prod128(p.mantissa(), a).pow2_64(p.mantissa_exponent(), true);
}
inline std::strong_ordering compare_fraction(Prod128 a, Prod128 b,
                                      Price p) { // compares a/b with p
  auto z{-p.mantissa_exponent()};
  auto pb{b * p.mantissa()};
  auto za{a.countl_zero()};
  auto zb{pb.countl_zero()};
  z -= za;
  z += zb - 64;
  if (z < 0)
    return std::strong_ordering::less;
  if (z > 0)
    return std::strong_ordering::greater;
  a <<= za;
  pb <<= zb;
  Prod192 pa{a, 0ull};
  return pa <=> pb;
}
