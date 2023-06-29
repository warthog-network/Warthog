#pragma once
#include "block/body/account_id.hpp"
#include<map>
class ChainDB;
class AddressFunds;
namespace chainserver {
struct AccountCache {
    AccountCache(const ChainDB& db)
        : db(db)
    {
    }

public:
    const AddressFunds& operator[](AccountId id);

private:
    std::map<AccountId, AddressFunds> map;
    const ChainDB& db;
};
}
