#pragma once

#include "block/body/transaction_id.hpp"
#include "entry.hpp"
#include <map>

class HashView;
class TransferTxExchangeMessageView;
namespace mempool {
class Txmap : public std::map<TransactionId, EntryValue, std::less<>> {
public:
    using map::map;
    struct AccountRange {
        Txmap& txmap;
        AccountId accountId;
        auto begin() { return txmap.lower_bound(accountId); }
        auto end() { return txmap.upper_bound(accountId); }
    };
    [[nodiscard]] std::vector<iterator> by_fee_inc(AccountId);

    AccountRange account_range(AccountId id) { return { *this, id }; }
};
struct ByFeeDesc {
    using iter_t = Txmap::iterator;
    void insert(iter_t iter);
    [[nodiscard]] size_t erase(iter_t iter);
    iter_t smallest() const { return data.back(); }
    std::vector<iter_t> sample(size_t n, size_t k) const;
    size_t size() const { return data.size(); }
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }

private:
    std::vector<iter_t> data;
};
}
