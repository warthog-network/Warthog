#include "cache.hpp"
#include "chainserver/db/chain_db.hpp"
#include "crypto/address.hpp"

namespace chainserver {

const AssetInfo& AssetCache::operator[](AssetId id)
{
    auto iter = map.find(id);
    if (iter != map.end())
        return iter->second;
    return map.emplace(id, db.fetch_asset(id)).first->second;
}

const Address& AddressCache::operator[](AccountId id)
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
