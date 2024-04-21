#pragma once
#include "block/body/account_id.hpp"
#include<map>
class ChainDB;
struct AddressFunds;
namespace chainserver {
class AccountCache {
public:
    AccountCache(const ChainDB& db)
        : db(db)
    {
    }

    const AddressFunds& operator[](AccountId id);

private:
    std::map<AccountId, AddressFunds> map;
    const ChainDB& db;
};
}
