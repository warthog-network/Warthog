#pragma once
#include "crypto/hash.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

class CustomFloat {
    struct Internal {
    private:
        constexpr void assert_nonzero_valid(uint64_t tmp) const
        {
            assert(tmp < (uint64_t(1ul) << 32));
            assert(tmp >= (uint64_t(1ul) << 31));
        }
        uint32_t _mantissa;
        int32_t _exponent;
        bool _positive { true };

        constexpr void set_nonzero_mantissa(uint64_t m)
        {
            assert_nonzero_valid(m);
            _mantissa = m;
        }
        constexpr Internal()
        {
            _mantissa = 0;
            _exponent = 0;
            _positive = true;
        }

    public:
        constexpr Internal(int32_t exponent, uint64_t mantissa,
            bool positive = true)
        {
            set_assert(exponent, mantissa);
            _positive = positive;
        }
        Internal(Hash h){
            _positive = true;
            int32_t exponent{0};
            size_t i = 0;
            for (; i < h.size(); ++i) {
                if (h[i] != 0)
                    break;
                exponent -= 8;
            }
            uint64_t tmpData{0};
            for (size_t j = 0;; ++j) {
                if (i < h.size())
                    tmpData |= h[i++];
                else
                    tmpData |= 0xFFu; // "infinite amount of trailing 1's"

                if (j >= 3)
                    break;
                tmpData <<= 8;
            }
            assert(tmpData != 0);
            shift_left(exponent,tmpData);
        }
        constexpr void set_exponent(int64_t e)
        {
            assert(e < int64_t(std::numeric_limits<int32_t>::max()));
            assert(e > int64_t(std::numeric_limits<int32_t>::min()));
            _exponent = e;
        }
        constexpr Internal& set_assert(int64_t e, uint64_t m)
        {
            set_exponent(e);
            if (m == 0)
                _mantissa = 0;
            else
                set_nonzero_mantissa(m);
            return *this;
        }
        static inline constexpr Internal new_shift_right(int32_t e, uint64_t m,
            bool positive = true)
        {
            return Internal().shift_right(e, m).set_positive(positive);
        }
        static inline constexpr Internal new_shift_left(int32_t e, uint64_t m,
            bool positive = true)
        {
            return Internal().shift_left(e, m).set_positive(positive);
        }
        constexpr Internal& shift_right(int32_t e, uint64_t m)
        {
            assert(m >= (uint64_t(1ul) << 31));
            assert(m != 0);
            while (m >= (uint64_t(1ul) << 32)) {
                m >>= 1;
                e += 1;
            }
            return set_assert(e, m);
        }
        constexpr Internal& shift_left(int32_t e, uint64_t m)
        {
            assert(m < (uint64_t(1ul) << 32));
            assert(m != 0);
            while (m < 0x80000000ul) {
                m <<= 1;
                e -= 1;
            }
            return set_assert(e, m);
        }
        constexpr Internal set_positive(bool positive)
        {
            _positive = positive;
            return *this;
        }
        void negate() { _positive = !_positive; }
        void set_zero() { _mantissa = 0; }
        auto mantissa() const { return _mantissa; }
        auto exponent() const { return _exponent; }
        auto positive() const { return _positive; }
    };

    friend inline bool operator<(const CustomFloat& arg, const CustomFloat& bound);
    

    // this function is for testing purposes
    static void assert_branch(int branch, int assertBranch)
    {
        assert(assertBranch == 0 || branch == assertBranch);
    }

private:
    Internal internal;
    constexpr CustomFloat(Internal i)
        : internal(i)
    {
    }
    static constexpr CustomFloat new_shift_left(int32_t e, uint64_t m,
        bool positive)
    {
        return { Internal::new_shift_left(e, m, positive) };
    }
    static CustomFloat new_shift_right(int32_t e, uint64_t m, bool positive)
    {
        return { Internal::new_shift_right(e, m, positive) };
    }
    static CustomFloat from_exponent(int64_t e, bool positive = true)
    {
        assert(e < int64_t(std::numeric_limits<int32_t>::max()) - 1);
        assert(e > int64_t(std::numeric_limits<int32_t>::min()) - 1);
        return CustomFloat(Internal(e + 1, 0x80000000ul, positive));
    }
    CustomFloat(int64_t exponent, uint64_t mantissa, bool positive = true)
        : internal(exponent, mantissa, positive)
    {
        assert(exponent <= std::numeric_limits<int32_t>::max());
        assert(exponent >= std::numeric_limits<int32_t>::min());
    }
    CustomFloat& add_exponent(int32_t add)
    {
        auto e { int64_t(internal.exponent()) + int64_t(add) };
        internal.set_exponent(e);
        return *this;
    }
    CustomFloat& negate_exponent()
    {
        auto e { int64_t(internal.exponent()) };
        internal.set_exponent(-e);
        return *this;
    }

public:
    auto is_zero() const { return internal.mantissa() == 0; }
    auto mantissa() const { return internal.mantissa(); }
    auto exponent() const { return internal.exponent(); }
    auto positive() const { return internal.positive(); }
    CustomFloat& operator-()
    {
        internal.negate();
        return *this;
    }
    CustomFloat& operator-=(CustomFloat cf) { return operator+=(-cf); }
    CustomFloat& operator+=(CustomFloat cf)
    {
        const auto e1 { exponent() };
        const auto e2 { cf.exponent() };

        if (is_zero())
            return *this = cf;
        if (cf.is_zero())
            return *this;
        if (e1 < e2) {
            std::swap(*this, cf);
            return *this += cf;
        }
        assert(e1 >= e2);
        if (e1 - e2 >= 64) {
            return *this;
        }
        assert(e1 - e2 < 64);
        uint64_t tmp { mantissa() };
        auto operand = uint64_t(cf.mantissa()) >> (e1 - e2);
        if (positive() == cf.positive()) {
            tmp += operand;
            assert(tmp <= uint64_t(1ul) << 33);
            internal.shift_right(e1, tmp);
        } else { // different signs
            if (operand == tmp) {
                internal.set_zero();
            } else if (operand > tmp) {
                assert(e1 == e2);
                internal.set_positive(cf.positive()); // change sign
                internal.shift_left(e2, operand - tmp);
            } else { // tmp > operand
                internal.shift_left(e1, tmp - operand);
            }
        }
        return *this;
    }
    CustomFloat operator+(CustomFloat f) const { return CustomFloat(*this) += f; }
    CustomFloat operator-(CustomFloat f) { return CustomFloat(*this) -= f; }

    // auto operator<=>(CustomFloat f) const{ // TODO
    //
    // }
    CustomFloat& operator*=(CustomFloat cf)
    {
        if (is_zero() || cf.is_zero()) {
            internal.set_zero();
            return *this;
        }
        internal.set_positive(positive() == cf.positive());
        const auto e1 { exponent() };
        const auto e2 { cf.exponent() };
        auto e { int64_t(e1) + int64_t(e2) };
        uint64_t tmp { uint64_t(mantissa()) * uint64_t(cf.mantissa()) };
        assert(tmp < 0xFFFFFFFF00000000ull);
        assert(tmp >= (uint64_t(1ul) << 62));
        if (tmp < (uint64_t(1ul) << 63)) {
            e -= 1;
            tmp <<= 1;
        }
        tmp >>= 32;
        internal.set_assert(e, tmp);
        return *this;
    }
    CustomFloat operator*(CustomFloat cf) const
    {
        return CustomFloat(*this) *= cf;
    }
    CustomFloat(const Hash& h)
        : internal(h)
    {
    }
    constexpr CustomFloat(int32_t exponent, uint64_t mantissa,
        bool positive = true)
        : internal(exponent, mantissa, positive)
    {
    }
    static CustomFloat zero() { return { 0, 0 }; }
    static CustomFloat from_double(double d)
    {
        assert(std::isfinite(d));
        if (d == 0)
            return zero();
        int e;
        auto r = std::frexp(d, &e);
        assert(std::isfinite(d) && r != 0.0);
        bool positive { r > 0.0 };
        if (r < 0.0) {
            r = -r;
        }
        r *= double(uint64_t(1ul) << 32);
        uint64_t m(r);
        return { e, m, positive };
    }
    static constexpr CustomFloat from_int(int32_t i)
    {
        if (i == 0) {
            return zero();
        }
        bool positive(i >= 0);
        if (i < 0) {
            i = -i;
        }
        assert(i > 0);
        uint32_t u(i);
        return new_shift_left(32, u, positive);
    }
    double to_double() const
    {
        if (is_zero()) return 0;
        double r = double(mantissa()) / double(uint64_t(1ul) << 32);
        assert(r < 1.0);
        assert(r >= 0.5);
        if (!positive())
            r = -r;
        return ldexp(r, exponent());
    }
    // logarithm to base 2
    friend CustomFloat log2(CustomFloat x)
    {
        assert(!x.is_zero());
        assert(x.positive());
        auto e { x.exponent() };
        using namespace std;
        x.internal.set_exponent(0);
        // These constants are from here: https://github.com/nadavrot/fast_log/blob/83bd112c330976c291300eaa214e668f809367ab/src/log_approx.cc#L56
        // The constants are given as constexpr in terms of exponent and mantissa explicitly because I am not sure
        // if CustomFloat::from_double(...) is platform dependend and influenced by flags like 
        // -ffast-math since from_double is using std::frexp internally. Furthermore std::frexp
        // is constexpr only from C++23 and I don't want to enforce this compiler requirement.
        constexpr auto c0 { CustomFloat(1, 2872373668ull) }; // = 1.33755322
        constexpr auto c1 { CustomFloat(3, 2377545675ull, false) }; // = -4.42852392
        constexpr auto c2 { CustomFloat(3, 3384280813ull) }; // = 6.30371424
        constexpr auto c3 { CustomFloat(2, 3451338727ull, false) }; // = -3.21430967
        auto d { c3 + x * (c2 + x * (c1 + x * c0)) };
        return CustomFloat::from_int(e) + d;
    }
    static CustomFloat pow2_fraction(CustomFloat f)
    {
        // I have modified constants from here: https://github.com/nadavrot/fast_log/blob/83bd112c330976c291300eaa214e668f809367ab/src/exp_approx.cc#L18
        // such that they don't compute the euler logartihm but logarithm to base 2.
        constexpr auto c0 { CustomFloat(-3, 3207796260ull) }; // = 0.09335915850659268
        constexpr auto c1 { CustomFloat(-2, 3510493713ull) }; // = 0.2043376277254389
        constexpr auto c2 { CustomFloat(0, 3014961390ull) }; // = 0.7019754011048444
        constexpr auto c3 { CustomFloat(1, 2147933481ull) }; // = 1.00020947
        auto d { c3 + f * (c2 + f * (c1 + f * c0)) };
        // assert( d <= CustomFloat::from_int(1) && d>= CustomFloat::zero()); //
        return d;
    }

    // power function to base 2
    friend CustomFloat pow2(CustomFloat x, int assertBranchForTesting = 0)
    {
        if (x.is_zero()) {
            assert_branch(1, assertBranchForTesting);
            return CustomFloat::from_int(1);
        }
        auto e_x { x.exponent() };

        // auto eex{-int64_t(x.exponent()) + 32};
        const auto m { x.mantissa() };
        assert(e_x <= 31); // overflow check
        if (e_x == 32) { // impossible by assert but would overflow anyway
            if (x.positive())
                return from_exponent(m);
            return from_exponent(-uint64_t(m));
        } else { // e_x < 32
            if (e_x > 0) { // e_x > 0
                assert(e_x < 32); // prevent undefined behavior for shifts
                const int64_t e(m >> (32 - e_x));
                auto m_frac { uint32_t(m) << (e_x) };
                if (m_frac == 0) {
                    assert_branch(2, assertBranchForTesting);
                    if (x.positive())
                        return from_exponent(e);
                    return from_exponent(e).negate_exponent().add_exponent(2);
                }
                auto frac { new_shift_left(0, m_frac, true) };
                assert_branch(3, assertBranchForTesting);
                if (x.positive())
                    return pow2_fraction(frac).add_exponent(e);
                return pow2_fraction(from_int(1) - frac)
                    .add_exponent(e - 1)
                    .negate_exponent();
            } else { // e_x <= 0
                assert(e_x <= 0);
                assert_branch(4, assertBranchForTesting);
                if (x.positive())
                    return pow2_fraction(x);
                return pow2_fraction(from_int(1) + x).negate_exponent().add_exponent(1);
            }
        }
    }
    // power function to arbitrary base
    friend CustomFloat pow(const CustomFloat& base, const CustomFloat& exponent)
    {
        return pow2(exponent * log2(base));
    }
};

inline bool operator<(const CustomFloat& arg, const CustomFloat& bound)
{
    assert(bound.positive());
    assert(bound.exponent() <= 0);
    uint32_t zerosBound(-bound.exponent());

    assert(arg.positive());
    assert(arg.exponent() <= 0);
    uint32_t zerosArg(-arg.exponent());
    if (zerosArg < zerosBound)
        return false;
    if (zerosArg > zerosBound)
        return true;
    return arg.mantissa() < bound.mantissa();
}
