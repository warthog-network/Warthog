#include "generator.hpp"
#include "block/body/view.hpp"
#include "block/header/view_inline.hpp"
HeaderGenerator::HeaderGenerator(std::array<uint8_t, 32> prevhash,
    const BodyView& bv, Target target,
    uint32_t timestamp)
    : version(target.is_janushash() ? 2 : 1)
    , prevhash(prevhash)
    , merkleroot(bv.merkleRoot())
    , timestamp(timestamp)
    , target(target)
    , nonce(0u) {

    };
[[nodiscard]] Header HeaderGenerator::serialize(uint32_t nonce) const
{
    Header out;
    uint32_t nversion = hton32(version);
    memcpy(out.data() + HeaderView::offset_version, &nversion, 4);
    memcpy(out.data() + HeaderView::offset_prevhash, prevhash.data(), 32);
    memcpy(out.data() + HeaderView::offset_merkleroot, merkleroot.data(), 32);
    uint32_t ntimestamp = hton32(timestamp);
    memcpy(out.data() + HeaderView::offset_timestamp, &ntimestamp, 4);
    uint32_t rawtarget{target.binary()};
    memcpy(out.data() + HeaderView::offset_target, &rawtarget, 4);
    memcpy(out.data() + HeaderView::offset_nonce, &nonce, 4);
    return out;
}

