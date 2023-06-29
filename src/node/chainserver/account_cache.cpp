#include "account_cache.hpp"
#include "db/chain_db.hpp"
#include "general/address_funds.hpp"

namespace chainserver {
const AddressFunds& AccountCache::operator[](AccountId id)
{
    auto iter = map.find(id);
    if (iter != map.end())
        return iter->second;
    auto p = db.fetch_account(id);
    return map.emplace(id, p).first->second;
}

}
