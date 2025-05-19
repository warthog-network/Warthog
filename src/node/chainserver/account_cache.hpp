#pragma once
#include "block/body/account_id.hpp"
#include "defi/token/info.hpp"
#include <map>
class ChainDB;
struct Address;
namespace chainserver {
class AccountCache {
public:
    AccountCache(const ChainDB& db)
        : db(db)
    {
    }

    const Address& operator[](AccountId id);

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

    const TokenInfo& operator[](TokenId id);

private:
    std::map<TokenId, TokenInfo> map;
    const ChainDB& db;
};

class DBCache {
public:
    DBCache(const ChainDB& db)
        : accounts(db)
        , tokens(db)
    {
    }
    AccountCache accounts;
    TokenCache tokens;
};

}
