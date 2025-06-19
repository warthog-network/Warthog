#pragma once
#include "block/header/header.hpp"
#include "block/header/pow_version.hpp"
#include "block/header/view_inline.hpp"

inline Target Header::target(NonzeroHeight h, bool testnet) const
{
    return static_cast<HeaderView>(*this).target(h, testnet);
}
inline bool Header::validPOW(const Hash& h, POWVersion v) const
{
    return static_cast<HeaderView>(*this).validPOW(h, v);
}
inline HashView Header::prevhash() const
{
    return static_cast<HeaderView>(*this).prevhash();
}
inline HashView Header::merkleroot() const
{
    return static_cast<HeaderView>(*this).merkleroot();
}
inline void Header::set_merkleroot(std::array<uint8_t, 32> a)
{
    memcpy(data() + HeaderView::offset_merkleroot, a.data(), 32);
}
inline BlockVersion Header::version() const
{
    return static_cast<HeaderView>(*this).version();
}
inline uint32_t Header::timestamp() const
{
    return static_cast<HeaderView>(*this).timestamp();
}
inline TargetV1 Header::target_v1() const
{
    return static_cast<HeaderView>(*this).target_v1();
}
inline TargetV2 Header::target_v2() const
{
    return static_cast<HeaderView>(*this).target_v2();
}
inline double Header::janus_number() const
{
    return static_cast<HeaderView>(*this).janus_number();
}
inline uint32_t Header::nonce() const
{
    return static_cast<HeaderView>(*this).nonce();
}
inline BlockHash Header::hash() const
{
    return static_cast<HeaderView>(*this).hash();
}
