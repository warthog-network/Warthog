#include "account_cache.hpp"
#include "chainserver/db/chain_db.hpp"
#include "crypto/address.hpp"

namespace chainserver {
const TokenInfo& TokenCache::operator[](TokenId id)
{
    auto iter = map.find(id);
    if (iter != map.end())
        return iter->second;
    auto p = db.fetch_token(id);
    return map.emplace(id, p).first->second;
}
const Address& AccountCache::operator[](AccountId id)
{
    auto iter = map.find(id);
    if (iter != map.end())
        return iter->second;
    auto p = db.fetch_address(id);
    return map.emplace(id, p).first->second;
}

}
