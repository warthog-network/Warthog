#include "block/header/custom_float.hpp"
#include <iostream>
using namespace std;

void assert_exactly_equal(double d1, double d2)
{
    assert(d1 == d2);
}
void assert_relative_almost_equal(double d1, double d2)
{
    if (d1 == 0.0) {
        assert(d2 == 0.0);
    } else {
        // check relative error:
        auto fraction = d2 / d1;
        assert(fraction > 0.9999999);
        assert(fraction < 1.0000001);
    }
}

void assert_absolute_somewhat_equal(double d1, double d2)
{
    if (d1 == 0.0) {
        assert(d2 < 0.003);
        assert(d2 > -0.003);
    } else {
        // check absolute error:
        assert(d1 - d2 < 0.003);
        assert(d2 - d1 < 0.003);
    }
}

void assert_relative_somewhat_equal(double d1, double d2)
{
    if (d1 == 0.0) {
        assert(d2 < 0.003);
        assert(d2 > -0.003);
    } else {
        auto fraction = d2 / d1;
        // check relative error:
        if (fraction < 0.995)
            cout << fraction << endl;
        if (fraction > 1.005)
            cout << fraction << " for d1=" << d1 << " and d2 = " << d2 << endl;
        assert(fraction > 0.995);
        assert(fraction < 1.005);
    }
}

void test_double_conversion(double d1)
{
    auto d2 { CustomFloat::from_double(d1).to_double() };
    assert_relative_almost_equal(d1, d2);
}

void test_int_conversion(int32_t i)
{
    auto d2 { CustomFloat::from_int(i).to_double() };
    assert_exactly_equal(i, d2);
}

void test_addition(double d1, double d2)
{
    auto a1 { d1 + d2 };
    auto a2 { (CustomFloat::from_double(d1) + CustomFloat::from_double(d2)).to_double() };
    assert_relative_almost_equal(a1, a2);
}

void test_multiplication(double d1, double d2)
{
    auto a1 { d1 * d2 };
    auto a2 { (CustomFloat::from_double(d1) * CustomFloat::from_double(d2)).to_double() };
    assert_relative_almost_equal(a1, a2);
}

void test_log2(double d)
{
    auto a1 { log2(d) };
    auto a2 { log2(CustomFloat::from_double(d)).to_double() };
    assert_absolute_somewhat_equal(a1, a2);
}
void test_pow2(double d)
{
    auto a1 { pow(2, d) };
    auto a2 { pow2(CustomFloat::from_double(d)).to_double() };
    assert_relative_somewhat_equal(a1, a2);
}

void test_pow(double base, double exponent)
{
    auto a1 { pow(base, exponent) };
    auto a2 { pow(CustomFloat::from_double(base), CustomFloat::from_double(exponent)).to_double() };
    assert_relative_somewhat_equal(a1, a2);
}

// in addition to fuzzy testing,
// explicitly test all 4 branches
// of the pow2 function
void test_pow2_branches()
{
    // the second parameter of pow2 asserts a branch.

    // branch 1 is for zero input
    {
        auto v { CustomFloat::from_double(0.0) };
        assert(v.is_zero());
        assert_exactly_equal(pow2(v, 1).to_double(), 1);
    }
    { // branch 2 is for positive exponent and no fractional part
        for (int i = 1; i < 100; ++i) {
            assert_relative_somewhat_equal(pow(2, i), pow2(CustomFloat::from_int(i), 2).to_double());
            assert_relative_somewhat_equal(pow(2, -i), pow2(CustomFloat::from_int(-i), 2).to_double());
        }
    }
    { // branch 3 is for positive exponent with fractional part
        for (int i = 1; i < 100; ++i) {
            auto v1 { double(i) * 1.123456 };
            assert_relative_somewhat_equal(pow(2, v1), pow2(CustomFloat::from_double(v1), 3).to_double());
            assert_relative_somewhat_equal(pow(2, -v1), pow2(CustomFloat::from_double(-v1), 3).to_double());
        }
    }
    { // branch 4 is for numbers in (-1, 1)
        for (int i = 1; i < 100; ++i) {
            auto v1 { double(i) / 101.0 };
            assert_relative_somewhat_equal(pow(2, v1), pow2(CustomFloat::from_double(v1), 4).to_double());
            assert_relative_somewhat_equal(pow(2, -v1), pow2(CustomFloat::from_double(-v1), 4).to_double());
        }
    }
}

void test_custom_float()
{
    double jitter { 0.001 };
    constexpr size_t bound { 100 };
    for (size_t i = 0; i < bound; ++i) {
        double d(i);
        test_int_conversion(int32_t(i));
        test_int_conversion(int32_t(i * i));
        test_int_conversion(-int32_t(i));
        test_int_conversion(-int32_t(i * i));
        test_double_conversion(d);
        test_double_conversion(d + jitter);
        test_double_conversion(d - jitter);
        test_double_conversion(-d);
        test_double_conversion(-d + jitter);
        test_double_conversion(-d - jitter);
        if (d != 0.0) {
            test_double_conversion(1 / d);
            test_double_conversion(-1 / d);

            // addition
            test_addition(d, 1 / d);
            test_addition(-d, 1 / d);
            test_addition(d, 2.123183);
            test_addition(d * d, d);

            // multiplication
            test_multiplication(d, 1 / d);
            test_multiplication(-d, 3.234 / d);
            test_multiplication(d, 2.123183);
            test_multiplication(d, 2.123183 * d);

            // test log2
            test_log2(d);
            test_log2(d + jitter);
            test_log2(d * d + jitter);
            test_log2(1 / d);
            test_log2(1 / d / (d + jitter));

            // test pow2
            test_pow2(d);
            test_pow2(1 / d);
            test_pow2(-d);
            test_pow2(-1 / d);

            // pow2 with positive jitter
            test_pow2(d + jitter);
            test_pow2(1 / d + jitter);
            test_pow2(-d + jitter);
            test_pow2(-1 / d + jitter);

            // pow2 with negative jitter
            test_pow2(d - jitter);
            test_pow2(1 / d - jitter);
            test_pow2(-d - jitter);
            test_pow2(-1 / d - jitter);

            // pow
            for (int j = 1; j < 11; ++j) {
                double exponent(double(j) / 10.0);
                test_pow(d, exponent);
                test_pow(1 / d, exponent);
            }
        }

        // extra test for values typically seen in mining
        for (int j = 1; j < 11; ++j) {
            double exponent(double(j) / 10.0);
            double hashAsNumber { pow(2, -d * 32.0 / bound) };
            test_pow(hashAsNumber, exponent);
        }
    }
    test_pow2(0.0);
    test_pow2_branches();
}

int main()
{
    auto f = CustomFloat::from_double(10);
    cout<<"exponent:  "<<f.exponent()<<endl;
    cout<<"mantissa:  "<<f.mantissa()<<endl;
    cout<<"to_double: "<<f.to_double()<<endl;
    test_custom_float();
    return 0;
}
