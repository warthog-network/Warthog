#pragma once
#include "general/params.hpp"
#include <cstddef>
#include <cstdint>
#include <variant>

// TODO: check latest TargetV1 and TargetV2 modifications
class Hash;
struct TargetV1 { // original target with 24 bit digits, 8 bit mantissa
    static constexpr uint32_t HARDESTTARGET_HOST = 0xFF800000u; // maximal target, 232 zeros then one digit 1 and 23 digits 0
    static constexpr uint32_t GENESISTARGET_HOST = (uint32_t(::GENESISDIFFICULTYEXPONENT) << 24) | 0x00FFFFFFu;
    static_assert(GENESISDIFFICULTYEXPONENT < 0xe8u);

private:
    uint32_t data;
    void set(uint32_t zeros, uint32_t bytes)
    {
        data = zeros << 24 | bytes;
    }

public:
    constexpr TargetV1(uint32_t data = 0u);
    TargetV1(double difficulty);

    bool operator!=(const TargetV1& t) const { return data != t.data; };
    bool operator==(const TargetV1&) const = default;

    uint32_t binary() const { return data; }
    // uint8_t& at(size_t index) { return ((uint8_t*)(&data))[index]; }
    // uint8_t& operator[](size_t index) { return at(index); }
    // uint8_t at(size_t index) const { return ((uint8_t*)(&data))[index]; }
    // uint8_t operator[](size_t index) const { return at(index); }
    uint32_t bits24() const;
    uint32_t zeros8() const;
    bool compatible(const Hash& hash) const;

    // scale target by easierfactor/harderfactor
    // easierfactor: int32_t which needs to be smaller than 0x80000000u because we multiply by 2 in the code
    // harderfactor: int32_t which needs to be smaller than 0x80000000u because we multiply by 2 in the code
    // These conditions should be fine because these factors will be based on seconds passed and 0x80000000u seconds are more than 60 years.
    void scale(uint32_t easierfactor, uint32_t harderfactor);
    double difficulty() const;

    static TargetV1 genesis();
};

class HashExponentialDigest;
struct TargetV2 { // new target with 22 bit digits, 20 bit mantissa to represent hash product even for small factors
    static constexpr uint32_t MaxTargetHost = 0xe00fffffu; // maximal target, 3*256 zeros then all 22 set to 1
    static constexpr uint8_t MinDiffExponent = 22; // 2^(<this number>) is the expected number of tries to mine the first
    static constexpr uint32_t MinTargetHost = (uint32_t(MinDiffExponent) << 24) | 0x003FFFFFu;
    static_assert(MinDiffExponent < 0xe8u);

private:
    uint32_t data;
    void set(uint32_t zeros, uint32_t bytes)
    {
        data = zeros << 22 | bytes;
    }

public:
    constexpr TargetV2(uint32_t data = 0u);
        TargetV2(double difficulty);

        bool operator!=(const TargetV2& t) const { return data != t.data; };
        bool operator==(const TargetV2&) const = default;

        uint32_t binary() const { return data; }
        // uint8_t& at(size_t index) { return ((uint8_t*)(&data))[index]; }
        // uint8_t& operator[](size_t index) { return at(index); }
        // uint8_t at(size_t index) const { return ((uint8_t*)(&data))[index]; }
        // uint8_t operator[](size_t index) const { return at(index); }
        uint32_t bits22() const;
        uint32_t zeros10() const;
        bool compatible(const HashExponentialDigest& digest) const;

        // scale target by easierfactor/harderfactor
        // easierfactor: int32_t which needs to be smaller than 0x80000000u because we multiply by 2 in the code
        // harderfactor: int32_t which needs to be smaller than 0x80000000u because we multiply by 2 in the code
        // These conditions should be fine because these factors will be based on seconds passed and 0x80000000u seconds are more than 60 years.
        void scale(uint32_t easierfactor, uint32_t harderfactor);
        double difficulty() const;
        static TargetV2 min();
};

class Target {
    public:
        Target(const auto& t)
            : t(t)
        {
        }
        auto difficulty() const
        {
            return std::visit([&](auto& t) { return t.difficulty(); }, t);
        }
        auto binary() const
        {
            return std::visit([&](auto& t) { return t.binary(); }, t);
        }
        bool is_janushash() const { return std::holds_alternative<TargetV2>(t); }
        const std::variant<TargetV1, TargetV2>& get() const { return t; };

        auto scale(uint32_t easierfactor, uint32_t harderfactor)
        {
            return std::visit([&](auto& t) { return t.scale(easierfactor, harderfactor); }, t);
        }
        bool operator==(const Target&) const = default;

    private:
        std::variant<TargetV1, TargetV2> t;
};
