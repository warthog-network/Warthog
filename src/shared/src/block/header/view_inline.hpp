#pragma once
#include "block/chain/height.hpp"
#include "block/header/header.hpp"
#include "crypto/hasher_sha256.hpp"
#include "crypto/verushash/verushash.hpp"
#include "difficulty.hpp"
#include "general/reader.hpp"

inline bool HeaderView::validPOW(const Hash& h, NonzeroHeight height) const
{
    if (JANUSENABLED && height.value() > JANUSRETARGETSTART){
        HashExponentialDigest hd; // prepare hash product of  Proof of Balanced work with two algos: verus + 3xsha256
        hd.digest(verus_hash({ data(), size() })); // verus hash v2.1
        hd.digest(hashSHA256(h)); // triple sha
        return target_v2().compatible(hd);
    } else {
        return target_v1().compatible(h);
    }
}
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

inline Target HeaderView::target(NonzeroHeight h) const
{
    if (JANUSENABLED && h.value() > JANUSRETARGETSTART)
        return target_v2();
    return target_v1();
}

inline TargetV1 HeaderView::target_v1() const
{
    uint32_t tmp;
    memcpy(&tmp, data() + offset_target, 4);
    return TargetV1(tmp);
}
inline TargetV2 HeaderView::target_v2() const
{
    uint32_t tmp;
    memcpy(&tmp, data() + offset_target, 4);
    return TargetV2(tmp);
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
