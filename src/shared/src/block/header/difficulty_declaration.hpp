#pragma once
#include "block/chain/height.hpp"
#include "general/byte_order.hpp"
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
    // static
    void set(uint32_t zeros, uint32_t bytes)
    {
        data = zeros << 24 | bytes;
    }

    constexpr TargetV1(uint32_t data)
        : data(data) {};

public:
    TargetV1()
        : data(0) {};
    static TargetV1 from_raw(const uint8_t*);
    TargetV1(double difficulty);

    bool operator!=(const TargetV1& t) const { return data != t.data; };
    bool operator==(const TargetV1&) const = default;

    uint32_t binary() const { return hton32(data); }
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
    void scale(uint32_t easierfactor, uint32_t harderfactor, Height);
    double difficulty() const;

    static TargetV1 genesis();
};

class HashExponentialDigest;
struct TargetV2 { // new target with 22 bit digits, 20 bit mantissa to represent hash product even for small factors
    static constexpr uint32_t MaxTargetHost = 0xe00fffffu; // maximal target, 3*256 zeros then all 22 set to 1
    // static_assert(MinDiffExponent < 0xe8u);

private:
    uint32_t data;
    void set(uint32_t zeros, uint32_t bytes)
    {
        data = zeros << 22 | bytes;
    }

    constexpr TargetV2(uint32_t data = 0u);

public:
    static TargetV2 from_raw(const uint8_t*);
    TargetV2(double difficulty);

    bool operator!=(const TargetV2& t) const { return data != t.data; };
    bool operator==(const TargetV2&) const = default;

    uint32_t binary() const { return hton32(data); }
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
    void scale(uint32_t easierfactor, uint32_t harderfactor, Height);
    double difficulty() const;
    static TargetV2 initial();
    static TargetV2 initialv2();
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

    auto scale(uint32_t easierfactor, uint32_t harderfactor, Height h)
    {
        return std::visit([&](auto& t) { return t.scale(easierfactor, harderfactor, h); }, t);
    }
    bool operator==(const Target&) const = default;

private:
    std::variant<TargetV1, TargetV2> t;
};
