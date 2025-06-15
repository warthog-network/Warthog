#pragma once
#include "block/body/account_id.hpp"
#include "block/chain/history/history.hpp"
#include "block/chain/history/index.hpp"
#include "chainserver/db/types_fwd.hpp"
#include "defi/token/info.hpp"
#include <map>
namespace chainserver {
class AccountCache {
public:
    AccountCache(const ChainDB& db)
        : db(db)
    {
    }

    [[nodiscard]] const Address& operator[](AccountId id);

private:
    std::map<AccountId, Address> map;
    const ChainDB& db;
};
class TokenCache {
public:
    TokenCache(const ChainDB& db)
        : db(db)
    {
    }

    [[nodiscard]] const TokenInfo& operator[](TokenId id);

private:
    std::map<TokenId, TokenInfo> map;
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
        : accounts(db)
        , tokens(db)
        , history(db)
    {
    }
    AccountCache accounts;
    TokenCache tokens;
    HistoryCache history;
};

}
