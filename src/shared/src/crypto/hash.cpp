#include "hash.hpp"
#include "general/params.hpp"
#include "hasher_sha256.hpp"

Hash Hash::genesis()
{
    return hashSHA256(reinterpret_cast<const uint8_t*>(GENESISSEED),
        strlen(GENESISSEED));
};
