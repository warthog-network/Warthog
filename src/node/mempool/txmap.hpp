#pragma once

#include "block/body/transaction_id.hpp"
#include "entry.hpp"
#include <map>

class HashView;
class TransferTxExchangeMessageView;
namespace mempool {
    class Txmap : public std::map<TransactionId, EntryValue, std::less<>>{
        public:
            using map::map;
            struct AccountRange {
                Txmap& txmap;
                AccountId accountId;
                auto begin(){return txmap.lower_bound(accountId);}
                auto end(){return txmap.upper_bound(accountId);}
            };
        AccountRange account_range(AccountId id){return {*this,id};}
    };
}
