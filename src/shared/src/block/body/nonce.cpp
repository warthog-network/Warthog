#include "nonce.hpp"
#include "block/chain/height.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/reader.hpp"
#include <algorithm>
#include <climits>
#include <random>
NonceId NonceId::random()
{
    std::array<uint8_t, 4> rand;
    std::independent_bits_engine<std::random_device, CHAR_BIT, uint8_t> e;
    std::generate(std::begin(rand), std::end(rand), std::ref(e));
    uint32_t val;
    memcpy(&val, rand.data(), rand.size());
    return NonceId(val);
};

PinNonce::PinNonce(ReaderCheck<bytesize> r)
    : id(r.r)
    , relativePin(r.r.uint8())
    , reserved(r.r) {};

PinNonce::PinNonce(Reader& r)
    : PinNonce(ReaderCheck<bytesize>(r)) {};

PinHeight PinNonce::pin_height(PinFloor pf) const
{
    Height h(pf - std::min(pin_offset(), pf.value()));
    assert(h.is_pin_height());
    return PinHeight(h);
}

std::optional<PinNonce> PinNonce::make_pin_nonce(NonceId nid, Height height, PinHeight pinHeight)
{
    PinFloor ph { height - 1 };
    if (ph < pinHeight)
        return {};
    uint64_t index = ((ph - pinHeight) >> 5);
    if (index > 255u)
        return {};
    return PinNonce(nid, (uint8_t)index);
}

// PinNonce

Writer& operator<<(Writer& w, const PinNonce& n)
{
    return w << n.id << n.relativePin << n.reserved;
};
