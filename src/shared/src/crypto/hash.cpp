#include "hash.hpp"
#include "general/hex.hpp"
#include "hasher_sha256.hpp"

std::string Hash::hex_string() const
{
    return serialize_hex(*this);
}

Hash Hash::genesis()
{
    return hashSHA256(reinterpret_cast<const uint8_t*>(GENESISSEED),
        strlen(GENESISSEED));
};
