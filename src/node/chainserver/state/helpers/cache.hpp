#pragma once
#include "block/body/account_id.hpp"
#include "block/chain/history/history.hpp"
#include "block/chain/history/index.hpp"
#include "chainserver/db/types_fwd.hpp"
#include "defi/token/info.hpp"
#include "general/address_funds.hpp"
#include <map>
namespace chainserver {
class AddressCache {
public:
    AddressCache(const ChainDB& db)
        : db(db)
    {
    }

    const std::optional<Address>& get(AccountId);
    const Address& get_throw(AccountId);
    const Address& fetch(AccountId);

private:
    std::map<AccountId, std::optional<Address>> map;
    const ChainDB& db;
};

class WartCache {
public:
    WartCache(const ChainDB& db)
        : db(db)
    {
    }
    Wart operator[](AccountId aid);

private:
    std::map<AccountId, Wart> map;
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
        , wart(db)
        , assets(db)
        , history(db)
    {
    }
    AddressCache addresses;
    WartCache wart;
    AssetCache assets;
    HistoryCache history;
};

}
