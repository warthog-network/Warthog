#pragma once

#include "block/body/transaction_id.hpp"
#include "entry.hpp"
#include <map>

class HashView;
class TransferTxExchangeMessageView;
namespace mempool {
class Txmap {
public:
    using map_t = std::map<TransactionId, EntryValue, std::less<>>;
    using const_iterator = map_t::const_iterator;
    using iterator = map_t::iterator;

private:
    map_t _map;
    int _cacheValidity { 0 }; // incremented on mempool change
public:
    auto cache_validity() const { return _cacheValidity; }

    auto size() const{return _map.size();}
    auto& operator()()
    {
        _cacheValidity += 1;
        return _map;
    }
    auto& operator()() const { return _map; }
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
