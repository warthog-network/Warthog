#pragma once
#include "general/view.hpp"
#include "pow_version.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
class HeaderGenerator;
class BlockVersion;
class HashView;
class TargetV1;
class TargetV2;
class Target;
class Hash;
class BlockHash;
class NonzeroHeight;
class Header;

class HeaderView : public View<80> {
    friend class HeaderGenerator;
    friend class Header;

public:
    static constexpr size_t offset_prevhash = 0ul;
    static constexpr size_t offset_target = 32ul;
    static constexpr size_t offset_merkleroot = 36ul;
    static constexpr size_t offset_version = 68ul;
    static constexpr size_t offset_timestamp = 72ul;
    static constexpr size_t offset_nonce = 76ul;
    static constexpr size_t bytesize = 80ul;
    using View::View;

    inline Target target(NonzeroHeight h, bool testnet) const;

    bool validPOW(const Hash& h, POWVersion v) const;

    double janus_number() const;
    Hash verus2_1_hash() const;
    Hash verus2_2_hash() const;
    inline BlockVersion version() const;
    inline HashView prevhash() const;
    inline HashView merkleroot() const;
    inline uint32_t timestamp() const;
    inline uint32_t nonce() const;
    BlockHash hash() const;
    bool operator==(const HeaderView rhs) const;
    bool operator==(const Header& arr) const;
    struct HeaderComparator {
        using arr80 = std::array<uint8_t, 80>;
        using is_transparent = std::true_type;
        bool operator()(const arr80& arr1, const arr80& arr2) const { return arr1 < arr2; }
        bool operator()(const arr80& arr, HeaderView hv) const
        {
            return memcmp(arr.data(), hv.data(), 80) < 0;
        }
        bool operator()(HeaderView hv, const arr80& arr) const
        {
            return memcmp(hv.data(), arr.data(), 80) < 0;
        }
    };

private:
    template <typename T>
    bool validPOW(const Hash& h) const;

private:
    TargetV1 target_v1() const;
    TargetV2 target_v2() const;
};
