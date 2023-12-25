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

inline bool operator<(const CustomFloat& arg, const CustomFloat bound)
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

bool HeaderView::validPOW(const Hash& h, NonzeroHeight height) const
{
    if (JANUSENABLED && height.value() > JANUSRETARGETSTART) {
        if (height.value() > JANUSV2RETARGETSTART) {
            auto verusHashV2_1 { verus_hash({ data(), size() }) };
            if (height.value() > JANUSV3RETARGETSTART) {
                if (!(verusHashV2_1 < CustomFloat(-30, 3496838790))) {
                    // reject verushash with log_e less than -21
                    return false;
                }
            }
            auto verusFloat { CustomFloat(verusHashV2_1) };
            using namespace std;
            // cout << to_bin(verusHashV2_1) << endl;
            auto sha256tFloat { CustomFloat(hashSHA256(h)) };
            // now introduce        _  ___  _Hacker: "shi*t" <-- At difficulty 2^40 which is minimum SHA256t
            // factor to cripple     \|o o|/                     must have 40/0.7 ~ 57 zeros to generate
            // GPU-only mining         \0/                       a valid block alone (2000x of 46 now)
            constexpr auto factor { CustomFloat(0, 3006477107) }; // = 0.7 <-- this can be decreased if necessary
            // constexpr auto factor { CustomFloat(0, 3435973836) }; // = 0.8, lift to this later when we have better miner
            auto hashProduct { verusFloat * pow(sha256tFloat, factor) };
            return verusHashV2_1[0] == 0 && (hashProduct < target_v2());

        } else { // Old Janushash
            // HashExponentialDigest hd; // prepare hash product of  Proof of Balanced work with two algos: verus + 3xsha256
            // auto verusHashV2_1 { verus_hash({ data(), size() }) };
            // hd.digest(verusHashV2_1);
            // auto triplesha { hashSHA256(h) };
            // hd.digest(triplesha);
            // return verusHashV2_1[0] == 0 && target_v2().compatible(hd);

            auto verusHashV2_1 { verus_hash({ data(), size() }) };
            auto verusFloat { CustomFloat(verusHashV2_1) };
            using namespace std;
            // cout << to_bin(verusHashV2_1) << endl;
            auto sha256tFloat { CustomFloat(hashSHA256(h)) };
            auto hashProduct { verusFloat * sha256tFloat };
            return verusHashV2_1[0] == 0 && (hashProduct < target_v2());
        }
    } else {
        return target_v1().compatible(h);
    }
}
