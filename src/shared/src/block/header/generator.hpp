#pragma once
#include "block/body/view_fwd.hpp"
#include "block/header/difficulty_declaration.hpp"
#include "block/header/header.hpp"
#include <array>
#include <cstdint>

class HeaderGenerator {
public:
    HeaderGenerator(std::array<uint8_t, 32> prevhash, const BodyView& bv,
        Target target,
        uint32_t timestamp, NonzeroHeight height);
    // member elements
    BlockVersion version; // 4 bytes
    std::array<uint8_t, 32> prevhash; // 32 bytes
    std::array<uint8_t, 32> merkleroot; // 32 bytes
    uint32_t timestamp; // 4 bytes
    Target target; // 4 bytes
    uint32_t nonce; // 4 bytes

    Header make_header(uint32_t nonce) const;
};
