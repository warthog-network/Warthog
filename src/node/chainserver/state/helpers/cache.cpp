#include "cache.hpp"
#include "chainserver/db/chain_db.hpp"
#include "crypto/address.hpp"

namespace chainserver {

const AssetDetail& AssetCacheById::operator[](AssetId id)
{
    auto iter = map.find(id);
    if (iter != map.end())
        return iter->second;
    return map.emplace(id, db.fetch_asset(id)).first->second;
}
const AssetDetail& AssetCacheByHash::operator[](AssetHash h)
{
    auto iter = map.find(h);
    if (iter != map.end())
        return iter->second;
    return map.emplace(h, db.fetch_asset(h)).first->second;
}

const std::optional<Address>& AddressCache::get(AccountId id)
{
    auto iter { map.find(id) };
    if (iter == map.end())
        iter = map.emplace(id, db.lookup_address(id)).first;
    return iter->second;
}

const Address& AddressCache::fetch(AccountId id)
{
    if (auto& o { get(id) })
        return o.value();
    throw std::runtime_error("Cannot fetch address with id" + std::to_string(id.value()) + ".");
}

Wart WartCache::operator[](AccountId aid)
{
    auto iter { map.find(aid) };
    if (iter == map.end())
        iter = map.emplace(aid, db.get_wart_balance(aid).second).first;
    return iter->second;
}

const history::Entry& HistoryCache::operator[](HistoryId id)
{
    auto iter = map.find(id);
    if (iter != map.end())
        return iter->second;
    return map.emplace(id, db.fetch_history(id)).first->second;
}

}
