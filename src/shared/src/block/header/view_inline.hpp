#pragma once

#include "block/chain/height.hpp"
#include "block/header/custom_float.hpp"
#include "block/header/header.hpp"
#include "crypto/hasher_sha256.hpp"
#include "crypto/verushash/verushash.hpp"
#include "difficulty.hpp"
#include "general/hex.hpp"
#include "general/reader.hpp"
#include <iostream>


inline uint32_t HeaderView::version() const
{
    return readuint32(data() + offset_version);
}
inline HashView HeaderView::prevhash() const
{
    return data() + offset_prevhash;
}
inline HashView HeaderView::merkleroot() const
{
    return data() + offset_merkleroot;
}
inline uint32_t HeaderView::timestamp() const
{
    return readuint32(data() + offset_timestamp);
}

inline Target HeaderView::target(NonzeroHeight h, bool testnet) const
{
    if (testnet || (JANUSENABLED && h.value() > JANUSRETARGETSTART))
        return target_v2();
    return target_v1();
}

inline TargetV1 HeaderView::target_v1() const
{
    return TargetV1::from_raw(data() + offset_target);
}
inline TargetV2 HeaderView::target_v2() const
{
    return TargetV2::from_raw(data() + offset_target);
}
inline uint32_t HeaderView::nonce() const
{
    return readuint32(data() + offset_nonce);
}
inline Hash HeaderView::hash() const
{
    auto h = hashSHA256(data(), bytesize);
    return hashSHA256(h.data(), 32);
}
inline bool HeaderView::operator==(const HeaderView rhs) const
{
    return std::memcmp(data(), rhs.data(), bytesize) == 0;
}
inline bool HeaderView::operator==(const Header& header) const
{
    return operator==(static_cast<HeaderView>(header));
}
