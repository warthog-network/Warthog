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

bool HeaderView::validPOW(const Hash& h, NonzeroHeight height, bool testnet) const
{
    if (testnet) {
        auto verusHashV2_1 { verus_hash() };
        auto verusFloat { CustomFloat(verusHashV2_1) };
        auto sha256tFloat { CustomFloat(hashSHA256(h)) };
        constexpr auto c = CustomFloat(-7, 2748779069); // 0.005
        if (sha256tFloat < c) {
            // we will adjust that if better miner is available
            sha256tFloat = c;
        }
        constexpr auto factor { CustomFloat(0, 3006477107) }; // = 0.7 <-- this can be decreased if necessary
        auto hashProduct { verusFloat * pow(sha256tFloat, factor) };
        return hashProduct < target_v2();
    }
    if (JANUSENABLED && height.value() > JANUSRETARGETSTART) {
        auto verusHashV2_1 { verus_hash() };
        if (height.value() > JANUSV2RETARGETSTART) {
            auto verusFloat { CustomFloat(verusHashV2_1) };
            auto sha256tFloat { CustomFloat(hashSHA256(h)) };
            if (height.value() > JANUSV4RETARGETSTART) {

                if (height.value() > JANUSV6RETARGETSTART) {
                    constexpr auto c = CustomFloat(-7, 2748779069); // 0.005
                    if (sha256tFloat < c) {
                        if (height.value() > JANUSV7RETARGETSTART)
                            return false;
                        else
                            sha256tFloat = c;
                    }
                }

                if (height.value() > JANUSV5RETARGETSTART) {
                    // temporary fix against unfair mining
                    // better GPU hashrate will also give more candidatess that pass through this threshold
                    // for verushash computation.
                    constexpr auto c = CustomFloat(-9, 3306097748); // CustomFloat::from_double(0.0015034391929775724)
                    if (sha256tFloat < c)
                        return false;
                }

                if (!(verusHashV2_1 < CustomFloat(-33, 3785965345))) {
                    // reject verushash with log_e less than -23
                    return false;
                }

            } else if (height.value() > JANUSV3RETARGETSTART) {
                if (!(verusHashV2_1 < CustomFloat(-30, 3496838790))) {
                    // reject verushash with log_e less than -21
                    return false;
                }
            }
            constexpr auto factor { CustomFloat(0, 3006477107) }; // = 0.7 <-- this can be decreased if necessary
            auto hashProduct { verusFloat * pow(sha256tFloat, factor) };
            return verusHashV2_1[0] == 0 && (hashProduct < target_v2());

        } else { // Old Janushash
            // HashExponentialDigest hd; // prepare hash product of  Proof of Balanced work with two algos: verus + 3xsha256
            // auto verusHashV2_1 { verus_hash({ data(), size() }) };
            // hd.digest(verusHashV2_1);
            // auto triplesha { hashSHA256(h) };
            // hd.digest(triplesha);
            // return verusHashV2_1[0] == 0 && target_v2().compatible(hd);

            auto verusFloat { CustomFloat(verusHashV2_1) };
            using namespace std;
            // cout << to_bin(verusHashV2_1) << endl;
            auto sha256tFloat { CustomFloat(hashSHA256(h)) };
            auto hashProduct { verusFloat * sha256tFloat };
            if (height.value() > JANUSV5RETARGETSTART) {
                // honest miners will be with 90% in a band around threshold, too good hashes are unlikely.
                // Here we reject best 10% of too good hashes, 90% of normally mined blocks will pass through.
                // This is to avoid unfair mining.
                if (hashProduct * CustomFloat(4, 2684354560) < target_v2())
                    return false;
            }
            return verusHashV2_1[0] == 0 && (hashProduct < target_v2());
        }
    } else {
        return target_v1().compatible(h);
    }
}
Hash HeaderView::verus_hash() const
{
    return ::verus_hash({ data(), size() });
}

double HeaderView::janus_number() const
{
    CustomFloat verusFloat { verus_hash() };
    CustomFloat sha256tFloat { hashSHA256(hashSHA256(hashSHA256(data(), size()))) };
    constexpr auto c = CustomFloat(-7, 2748779069);
    if (sha256tFloat < c)
        return 1.0;

    constexpr auto factor { CustomFloat(0, 3006477107) };
    return (verusFloat * pow(sha256tFloat, factor)).to_double();
}
