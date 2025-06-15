#include "account_cache.hpp"
#include "chainserver/db/chain_db.hpp"
#include "crypto/address.hpp"

namespace chainserver {

const TokenInfo& TokenCache::operator[](TokenId id)
{
    auto iter = map.find(id);
    if (iter != map.end())
        return iter->second;
    return map.emplace(id, db.fetch_token(id)).first->second;
}

const Address& AccountCache::operator[](AccountId id)
{
    auto iter = map.find(id);
    if (iter != map.end())
        return iter->second;
    return map.emplace(id, db.fetch_address(id)).first->second;
}

const history::Entry& HistoryCache::operator[](HistoryId id)
{
    auto iter = map.find(id);
    if (iter != map.end())
        return iter->second;
    return map.emplace(id, db.fetch_history(id)).first->second;
}

}
