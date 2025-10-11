#pragma once
#include "block/body/account_id.hpp"
#include "block/chain/history/history.hpp"
#include "block/chain/history/index.hpp"
#include "chainserver/db/types_fwd.hpp"
#include "defi/token/account_token.hpp"
#include "defi/token/info.hpp"
#include <map>

namespace chainserver {

template <typename Key, typename Value>
class DBCacheBase {
public:
    DBCacheBase(const ChainDB& db)
        : db(db)
    {
    }
    void clear() { map.clear(); }

protected:
    const ChainDB& db;
    mutable std::map<Key, Value> map;
};

class AddressCache : public DBCacheBase<AccountId, std::optional<Address>> {
public:
    using DBCacheBase::DBCacheBase;

    const std::optional<Address>& get(AccountId);
    const Address& fetch(AccountId);
};

class BalanceCache : public DBCacheBase<AccountToken, Funds_uint64> {
public:
    using DBCacheBase::DBCacheBase;
    [[nodiscard]] Funds_uint64 operator[](AccountToken at);
};

class AssetCacheById : public DBCacheBase<AssetId, AssetDetail> {
public:
    using DBCacheBase::DBCacheBase;
    [[nodiscard]] const AssetDetail& operator[](AssetId id);
};

class AssetCacheByHash : public DBCacheBase<AssetHash, AssetDetail> {
public:
    using DBCacheBase::DBCacheBase;
    [[nodiscard]] const AssetDetail& fetch(AssetHash);
    [[nodiscard]] const AssetDetail* lookup(AssetHash);
};

class HistoryCache : public DBCacheBase<HistoryId, history::Entry> {
public:
    using DBCacheBase::DBCacheBase;
    [[nodiscard]] const history::Entry& operator[](HistoryId id);
};

class DBCache {
public:
    DBCache(const ChainDB& db)
        : addresses(db)
        , balance(db)
        , assetsById(db)
        , assetsByHash(db)
        , history(db)
    {
    }
    void clear()
    {
        addresses.clear();
        balance.clear();
        assetsById.clear();
        assetsByHash.clear();
        history.clear();
    }

    AddressCache addresses;
    BalanceCache balance;
    AssetCacheById assetsById;
    AssetCacheByHash assetsByHash;
    HistoryCache history;
};

}
