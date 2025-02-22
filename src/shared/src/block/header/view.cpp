#include "view.hpp"
#include "block/chain/height.hpp"
#include "block/header/header_impl.hpp"
#include "crypto/hasher_sha256.hpp"
#include "crypto/verushash/verushash.hpp"
#include "custom_float.hpp"
#include "difficulty.hpp"
#include "general/params.hpp"
#include <iostream>

inline bool operator<(const CustomFloat& hashproduct, TargetV2 t)
{
    auto zerosTarget { t.zeros10() };
    assert(hashproduct.positive());
    assert(hashproduct.exponent() <= 0);
    uint32_t zerosHashproduct(-hashproduct.exponent());
    if (zerosTarget > zerosHashproduct)
        return false;
    uint64_t bits32 { t.bits22() << 10 };
    // if (zerosHashproduct - zerosTarget < 64) { // avoid undefined behavior in shift
    //     double bound(bits32 << (zerosHashproduct - zerosTarget));
    //     double val(hashproduct.mantissa());
    //     spdlog::info("fraction: {}", val / bound);
    // }
    if (zerosTarget < zerosHashproduct)
        return true;
    return hashproduct.mantissa() < bits32;
}

[[nodiscard]] inline std::string to_bin(Hash v)
{
    std::string s;
    s.resize(256);
    for (size_t i = 0; i < 32; ++i) {
        uint8_t u = ((uint8_t*)(&v))[i];
        auto p = s.data() + 8 * i;
        for (int j = 0; j < 8; ++j) {
            p[j] = ((u & 1 << (7 - j)) != 0 ? '1' : '0');
        }
    }
    return "0b_" + s;
}


template <>
bool HeaderView::validPOW<POWVersion::Original>(const Hash& h) const
{
    return target_v1().compatible(h);
}

template <>
bool HeaderView::validPOW<POWVersion::Janus1>(const Hash& h) const
{
    auto verusHash { verus2_1_hash() };
    auto verusFloat { CustomFloat(verusHash) };
    auto sha256tFloat { CustomFloat(hashSHA256(h)) };
    auto hashProduct { verusFloat * sha256tFloat };
    return verusHash[0] == 0 && (hashProduct < target_v2());
}

template <>
bool HeaderView::validPOW<POWVersion::Janus2>(const Hash& h) const
{
    auto verusHash { verus2_1_hash() };
    auto verusFloat { CustomFloat(verusHash) };
    auto sha256tFloat { CustomFloat(hashSHA256(h)) };
    constexpr auto factor { CustomFloat(0, 3006477107) }; // = 0.7 <-- this can be decreased if necessary
    auto hashProduct { verusFloat * pow(sha256tFloat, factor) };
    return verusHash[0] == 0 && (hashProduct < target_v2());
}

template <>
bool HeaderView::validPOW<POWVersion::Janus3>(const Hash& h) const
{
    auto verusHash { verus2_1_hash() };
    auto verusFloat { CustomFloat(verusHash) };
    auto sha256tFloat { CustomFloat(hashSHA256(h)) };
    constexpr auto factor { CustomFloat(0, 3006477107) };
    auto hashProduct { verusFloat * pow(sha256tFloat, factor) };
    if (!(verusHash < CustomFloat(-30, 3496838790))) {
        // reject verushash with log_e not less than -21
        return false;
    }
    return verusHash[0] == 0 && (hashProduct < target_v2());
}

template <>
bool HeaderView::validPOW<POWVersion::Janus4>(const Hash& h) const
{
    auto verusHash { verus2_1_hash() };
    auto verusFloat { CustomFloat(verusHash) };
    auto sha256tFloat { CustomFloat(hashSHA256(h)) };
    constexpr auto factor { CustomFloat(0, 3006477107) };
    auto hashProduct { verusFloat * pow(sha256tFloat, factor) };
    if (!(verusHash < CustomFloat(-33, 3785965345))) {
        // reject verushash with log_e not less than -23
        return false;
    }
    return verusHash[0] == 0 && (hashProduct < target_v2());
}

template <>
bool HeaderView::validPOW<POWVersion::Janus5>(const Hash& h) const
{
    auto verusHash { verus2_1_hash() };
    auto verusFloat { CustomFloat(verusHash) };
    auto sha256tFloat { CustomFloat(hashSHA256(h)) };
    constexpr auto factor { CustomFloat(0, 3006477107) };
    auto hashProduct { verusFloat * pow(sha256tFloat, factor) };
    if (!(verusHash < CustomFloat(-33, 3785965345))) {
        // reject verushash with log_e less than -23
        return false;
    }
    constexpr auto c = CustomFloat(-9, 3306097748); // CustomFloat::from_double(0.0015034391929775724)
    if (sha256tFloat < c)
        return false;
    return verusHash[0] == 0 && (hashProduct < target_v2());
}

template <>
bool HeaderView::validPOW<POWVersion::Janus6>(const Hash& h) const
{
    auto verusFloat { CustomFloat(verus2_1_hash()) };
    auto sha256tFloat { CustomFloat(hashSHA256(h)) };
    constexpr auto c = CustomFloat(-7, 2748779069); // 0.005
    if (sha256tFloat < c) {
        sha256tFloat = c;
    }
    constexpr auto factor { CustomFloat(0, 3006477107) };
    auto hashProduct { verusFloat * pow(sha256tFloat, factor) };
    return hashProduct < target_v2();
}

template <>
bool HeaderView::validPOW<POWVersion::Janus7>(const Hash& h) const
{
    auto verusFloat { CustomFloat(verus2_1_hash()) };
    auto sha256tFloat { CustomFloat(hashSHA256(h)) };
    {
        constexpr auto c = CustomFloat(-7, 2748779069); // 0.005
        if (sha256tFloat < c) {
            return false;
        }
    }
    constexpr auto factor { CustomFloat(0, 3006477107) };
    auto hashProduct { verusFloat * pow(sha256tFloat, factor) };
    return hashProduct < target_v2();
}

template <>
bool HeaderView::validPOW<POWVersion::Janus8>(const Hash& h) const
{
    auto verusFloat { CustomFloat(verus2_2_hash()) };
    auto sha256tFloat { CustomFloat(hashSHA256(h)) };
    {
        constexpr auto c = CustomFloat(-7, 2748779069); // 0.005
        if (sha256tFloat < c) {
            return false;
        }
    }
    constexpr auto factor { CustomFloat(0, 3006477107) };
    auto hashProduct { verusFloat * pow(sha256tFloat, factor) };
    return hashProduct < target_v2();
}

bool HeaderView::validPOW(const Hash& h, POWVersion version) const
{
    return version.visit([this, &h](auto version) -> bool {
        return validPOW<std::remove_cv_t<decltype(version)>>(h);
    });
}

Hash HeaderView::verus2_1_hash() const
{
    return ::verus_hash_v2_1({ data(), size() });
}

Hash HeaderView::verus2_2_hash() const
{
    return ::verus_hash_v2_2({ data(), size() });
}

double HeaderView::janus_number() const
{
    CustomFloat verusFloat { version().value() == 2 ? verus2_1_hash() : verus2_2_hash() };
    CustomFloat sha256tFloat { hashSHA256(hashSHA256(hashSHA256(data(), size()))) };
    constexpr auto c = CustomFloat(-7, 2748779069);
    if (sha256tFloat < c)
        return 1.0;

    constexpr auto factor { CustomFloat(0, 3006477107) };
    return (verusFloat * pow(sha256tFloat, factor)).to_double();
}
