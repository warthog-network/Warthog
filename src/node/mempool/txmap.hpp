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
    [[nodiscard]] std::vector<const_iterator> by_fee_inc(AccountId) const;
};
struct ByFeeDesc {
    using const_iter_t = Txmap::const_iterator;
    bool insert(const_iter_t iter);
    [[nodiscard]] size_t erase(const_iter_t iter);
    const_iter_t smallest() const { return data.back(); }
    std::vector<const_iter_t> sample(size_t n, size_t k) const;
    size_t size() const { return data.size(); }
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }

private:
    std::vector<const_iter_t> data;
};
}
