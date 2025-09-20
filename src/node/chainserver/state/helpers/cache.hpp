#pragma once
#include "block/body/account_id.hpp"
#include "block/chain/history/history.hpp"
#include "block/chain/history/index.hpp"
#include "chainserver/db/types_fwd.hpp"
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

class WartCache : public DBCacheBase<AccountId, Wart> {
public:
    using DBCacheBase::DBCacheBase;
    [[nodiscard]] Wart operator[](AccountId aid);
};

class AssetCacheById : public DBCacheBase<AssetId, AssetDetail> {
public:
    using DBCacheBase::DBCacheBase;
    [[nodiscard]] const AssetDetail& operator[](AssetId id);
};

class AssetCacheByHash : public DBCacheBase<AssetHash, AssetDetail> {
public:
    using DBCacheBase::DBCacheBase;
    [[nodiscard]] const AssetDetail& operator[](AssetHash);
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
        , wart(db)
        , assetsById(db)
        , assetsByHash(db)
        , history(db)
    {
    }
    void clear()
    {
        addresses.clear();
        wart.clear();
        assetsById.clear();
        assetsByHash.clear();
        history.clear();
    }

    AddressCache addresses;
    WartCache wart;
    AssetCacheById assetsById;
    AssetCacheByHash assetsByHash;
    HistoryCache history;
};

}
