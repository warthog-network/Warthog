#include "worksum.hpp"
#include "block/header/difficulty.hpp"
#include "general/hex.hpp"
#include "general/reader.hpp"

std::string Worksum::to_string() const
{
    std::string out;
    out.resize(66);
    memcpy(out.data(), "0x", 2);
    static_assert(std::tuple_size<decltype(fragments)>::value == 8);
    for (size_t i = 0; i < fragments.size(); ++i) {
        serialize_hex(fragments[7 - i], out.data() + 2 + i * 8);
    }
    return out;
};

Worksum& Worksum::operator*=(uint32_t factor)
{
    uint64_t carry = 0;
    for (size_t i = 0; i < fragments.size(); i++) {
        uint64_t n = carry + uint64_t(fragments[i]) * uint64_t(factor);
        fragments[i] = n & 0xfffffffful;
        carry = n >> 32;
    }
    return *this;
};
Worksum& Worksum::operator+=(const Worksum& w)
{
    uint64_t carry = 0;
    for (size_t i = 0; i < fragments.size(); i++) {
        uint64_t n = carry + uint64_t(fragments[i]) + uint64_t(w.fragments[i]);
        fragments[i] = n & 0xfffffffful;
        carry = n >> 32;
    }
    return *this;
}
Worksum& Worksum::operator-=(const Worksum& w)
{
    uint64_t carry = 0;
    for (size_t i = 0; i < fragments.size(); i++) {
        carry += w.fragments[i];
        if (fragments[i] >= carry) {
            fragments[i] = (fragments[i] - carry) & 0xfffffffful;
            carry = 0;
        } else {
            fragments[i] = (fragments[i] - carry) & 0xfffffffful;
            carry = 1;
        }
    }
    return *this;
}

Worksum::Worksum(std::array<uint8_t, 32> data)
{
    for (size_t i = 0; i < fragments.size(); ++i) {
        uint32_t f = readuint32(data.data() + i * 4);
        fragments[i] = f;
    }
};

Worksum::Worksum(const TargetV1& t)
{
    fragments.fill(0ul);
    uint32_t zeros = t.zeros8();
    uint64_t invbits = (uint64_t(1) << (24 + 31)) / uint64_t(t.bits24());
    // 2^31<invbits<=2^32
    if (invbits == (uint64_t(1)<<32)) { // 2^32
        zeros += 1;
        size_t fragmentindex = zeros / 32;
        uint8_t shift = zeros & 0x1F; // zeros % 32
        fragments[fragmentindex] = 1 << shift;
    } else { // 2^31<invbits<2^32
        size_t fragmentindex = zeros / 32;
        uint8_t shift = zeros & 0x1F; // zeros % 32
        fragments[fragmentindex] = (invbits >> (31 - shift));
        if (fragmentindex > 0) {
            fragments[fragmentindex - 1] = (invbits << (1 + shift));
        }
    }
}

Worksum::Worksum(const TargetV2& t)
{
    fragments.fill(0ul);
    uint32_t zeros = t.zeros10();
    uint64_t invbits = (uint64_t(1) << (22 + 31)) / uint64_t(t.bits22());
    // 2^31<invbits<=2^32
    if (invbits == (uint64_t(1)<<32)) { // 2^32
        zeros += 1;
        size_t fragmentindex = zeros / 32;
        uint8_t shift = zeros & 0x1F; // zeros % 32
        fragments[fragmentindex] = 1 << shift;
    } else { // 2^31<invbits<2^32
        size_t fragmentindex = zeros / 32;
        uint8_t shift = zeros & 0x1F; // zeros % 32
        fragments[fragmentindex] = (invbits >> (31 - shift));
        if (fragmentindex > 0) {
            fragments[fragmentindex - 1] = (invbits << (1 + shift));
        }
    }
}

Worksum::Worksum(const Target& t)
{
    *this = std::visit([&](const auto& t) -> Worksum { return t; }, t.get());
}

std::array<uint8_t, 32> Worksum::to_bytes() const
{
    std::array<uint8_t, 32> res;
    for (size_t i = 0; i < fragments.size(); ++i) {
        uint32_t f = hton32(fragments[i]);
        memcpy(res.data() + 4 * i, &f, sizeof(f));
    }
    return res;
};

