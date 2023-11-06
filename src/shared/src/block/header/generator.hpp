#pragma once

#include "block/header/difficulty.hpp"
#include "block/header/header.hpp"
#include "general/now.hpp"
#include <array>
#include <cstdint>
class BodyView;
class HeaderGenerator {
public:
    HeaderGenerator(std::array<uint8_t, 32> prevhash, const BodyView& bv,
        Target target,
        uint32_t timestamp);
    // member elements
    int32_t version = 1; // 4 bytes
    std::array<uint8_t, 32> prevhash; // 32 bytes
    std::array<uint8_t, 32> merkleroot; // 32 bytes
    uint32_t timestamp; // 4 bytes
    Target target; // 4 bytes
    uint32_t nonce; // 4 bytes

    //
    Header serialize(uint32_t nonce) const;
};
