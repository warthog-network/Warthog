#pragma once
#include "block/header/header.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/reader.hpp"
#include "difficulty.hpp"

inline bool HeaderView::validPOW() const
{
    return target().compatible(hash());
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
inline Target HeaderView::target() const
{
    uint32_t tmp;
    memcpy(&tmp, data() + offset_target, 4);
    return Target(tmp);
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
