#pragma once
#include "block/body/account_id.hpp"
#include "block/chain/history/history.hpp"
#include "block/chain/history/index.hpp"
#include "chainserver/db/types_fwd.hpp"
#include "defi/token/info.hpp"
#include <map>
namespace chainserver {
class AddressCache {
public:
    AddressCache(const ChainDB& db)
        : db(db)
    {
    }

    [[nodiscard]] const Address& operator[](AccountId id);

private:
    std::map<AccountId, Address> map;
    const ChainDB& db;
};
class AssetCache {
public:
    AssetCache(const ChainDB& db)
        : db(db)
    {
    }

    [[nodiscard]] const AssetInfo& operator[](AssetId id);

private:
    std::map<AssetId, AssetInfo> map;
    const ChainDB& db;
};

class HistoryCache {
public:
    HistoryCache(const ChainDB& db)
        : db(db)
    {
    }

    [[nodiscard]] const history::Entry& operator[](HistoryId id);

private:
    std::map<HistoryId, history::Entry> map;
    const ChainDB& db;
};

class DBCache {
public:
    DBCache(const ChainDB& db)
        : addresses(db)
        , assets(db)
        , history(db)
    {
    }
    AddressCache addresses;
    AssetCache assets;
    HistoryCache history;
};

}
