#pragma once
#include "defi/token/account_token.hpp"
#include "general/funds.hpp"
#include <map>
namespace chainserver {
struct FreeBalanceUpdates {
    std::map<AccountToken, Funds_uint64> wart;
    std::map<AccountToken, Funds_uint64> nonWart;
    void insert_or_assign(AccountToken at, Funds_uint64 f){
        if (at.token_id().is_wart()) 
            wart.insert_or_assign(at, f);
        else
            nonWart.insert_or_assign(at, f);
    }
    void merge(FreeBalanceUpdates&& b)
    {
        wart.merge(std::move(b.wart));
        nonWart.merge(std::move(b.nonWart));
    }
};
}
