#include "generator.hpp"
#include "block/body/view.hpp"
#include "block/header/view_inline.hpp"
HeaderGenerator::HeaderGenerator(std::array<uint8_t, 32> prevhash,
                                 const BodyView &bv, Target target,
                                 uint32_t timestamp, uint32_t version)
    : version(version), prevhash(prevhash), merkleroot(bv.merkleRoot()),
      timestamp(timestamp), target(target), nonce(0u){

                                            };
[[nodiscard]] Header HeaderGenerator::serialize(uint32_t nonce) const {
  Header out;
  uint32_t nversion = htonl(version);
  memcpy(out.data() + HeaderView::offset_version, &nversion, 4);
  memcpy(out.data() + HeaderView::offset_prevhash, prevhash.data(), 32);
  memcpy(out.data() + HeaderView::offset_merkleroot, merkleroot.data(), 32);
  uint32_t ntimestamp = htonl(timestamp);
  memcpy(out.data() + HeaderView::offset_timestamp, &ntimestamp, 4);
  memcpy(out.data() + HeaderView::offset_target, &target, 4);
  memcpy(out.data() + HeaderView::offset_nonce, &nonce, 4);
  return out;
}

bool HeaderGenerator::mine(std::array<uint8_t, 80> &out, size_t maxtries) {
  out = serialize(0);
  HeaderView hv(out.data());
  uint32_t nonce;
  memcpy(&nonce, out.data() + HeaderView::offset_version, 4);
  for (size_t i = 0; i < maxtries; ++i) {
    // check hash
    memcpy(out.data() + HeaderView::offset_nonce, &nonce, 4);
    if (target.compatible(hv.hash()))
      return true;
    nonce += 1;
  }
  return false;
}
