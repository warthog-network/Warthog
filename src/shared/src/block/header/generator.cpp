#include "generator.hpp"
#include "block/body/view.hpp"
#include "block/header/view_inline.hpp"
#include "general/is_testnet.hpp"
namespace {
uint32_t header_version(NonzeroHeight h)
{
    if (is_testnet()) {
        if (h.value() <= 2)
            return 2;
        return 3;
    } else {
        if (h.value() <= JANUSV8BLOCKV3START)
            return 2;
        return 3;
    }
}
}
HeaderGenerator::HeaderGenerator(std::array<uint8_t, 32> prevhash,
    const BodyView& bv, Target target,
    uint32_t timestamp, NonzeroHeight height)
    : version(target.is_janushash() ? header_version(height) : 1)
    , prevhash(prevhash)
    , merkleroot(bv.merkle_root(height))
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
    uint32_t rawtarget { target.binary() };
    memcpy(out.data() + HeaderView::offset_target, &rawtarget, 4);
    memcpy(out.data() + HeaderView::offset_nonce, &nonce, 4);
    return out;
}
