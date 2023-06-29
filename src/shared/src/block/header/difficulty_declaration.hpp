#pragma once
#include <cstddef>
#include <cstdint>

class Hash;
struct Target {
private:
    uint32_t data;

public:
    constexpr Target(uint32_t data = 0u)
        : data(data) {};
    Target(double difficulty);

    bool operator!=(const Target& t) const { return data != t.data; };
    bool operator==(const Target&) const = default;

    uint32_t binary() const { return data; }
    uint8_t& at(size_t index) { return ((uint8_t*)(&data))[index]; }
    uint8_t& operator[](size_t index) { return at(index); }
    uint8_t at(size_t index) const { return ((uint8_t*)(&data))[index]; }
    uint8_t operator[](size_t index) const { return at(index); }
    uint32_t bits() const;
    bool compatible(const Hash& hash) const;

    // scale target by easierfactor/harderfactor
    // easierfactor: int32_t which needs to be smaller than 0x80000000u because we multiply by 2 in the code
    // harderfactor: int32_t which needs to be smaller than 0x80000000u because we multiply by 2 in the code
    // These conditions should be fine because these factors will be based on seconds passed and 0x80000000u seconds are more than 60 years.
    void scale(uint32_t easierfactor, uint32_t harderfactor);
    double difficulty() const;

    static Target genesis();
};
