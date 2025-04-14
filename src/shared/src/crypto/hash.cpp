#include "hash.hpp"
#include "general/params.hpp"
#include "general/hex.hpp"
#include "hasher_sha256.hpp"

std::string Hash::hex_string() const
{
    return serialize_hex(*this);
}

std::optional<Hash> Hash::parse_string(std::string_view hex){
    auto h{uninitialized()};
    if (parse_hex(hex, h))
        return h;
    return {};
}

Hash Hash::genesis()
{
    return hashSHA256(reinterpret_cast<const uint8_t*>(GENESISSEED),
        strlen(GENESISSEED));
};
