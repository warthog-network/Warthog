#pragma once
#include "block/header/header.hpp"
#include "block/header/view_inline.hpp"

inline Target Header::target(NonzeroHeight h) const
{
    return static_cast<HeaderView>(*this).target(h);
}
inline bool Header::validPOW(const Hash& h, NonzeroHeight height) const
{
    return static_cast<HeaderView>(*this).validPOW(h,height);
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
inline void Header::set_nonce(uint32_t nonce)
{
    memcpy(data() + HeaderView::offset_nonce, &nonce, 4);
}
inline uint32_t Header::version() const
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
inline uint32_t Header::nonce() const
{
    return static_cast<HeaderView>(*this).nonce();
}
inline Hash Header::hash() const
{
    return static_cast<HeaderView>(*this).hash();
}
