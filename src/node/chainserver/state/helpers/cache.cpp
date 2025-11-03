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
const AssetDetail& AssetCacheByHash::fetch(AssetHash h)
{
    auto iter = map.find(h);
    if (iter != map.end())
        return iter->second;
    return map.emplace(h, db.fetch_asset(h)).first->second;
}
const AssetDetail* AssetCacheByHash::lookup(AssetHash h)
{
    auto iter = map.find(h);
    if (iter != map.end())
        return &iter->second;
    if (auto a { db.lookup_asset(h) })
        return &map.emplace(h, *a).first->second;
    return nullptr;
}

const wrt::optional<Address>& AddressCache::get(AccountId id)
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

Funds_uint64 BalanceCache::operator[](AccountToken at)
{
    auto iter { map.find(at) };
    if (iter == map.end())
        iter = map.emplace(at, db.get_free_balance(at).second).first;
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
