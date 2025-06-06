#pragma once

#include "block/body/messages.hpp"
#include "block/body/transaction_id.hpp"
#include "entry.hpp"
#include <set>

class HashView;
namespace mempool {
struct ComparatorTransactionId {
    using is_transparent = std::true_type;
    bool operator()(const TransactionMessage& m1, const TransactionId& txid2)
    {
        return m1.txid() < txid2;
    }
    bool operator()(const TransactionMessage& m1, const TransactionMessage& m2)
    {
        return m1.txid() < m2.txid();
    }
    bool operator()(const TransactionId& txid1, const TransactionMessage& m2)
    {
        return txid1 < m2.txid();
    }
};
class Txset {
public:
    using set_t = std::set<TransactionMessage, ComparatorTransactionId>;
    using const_iterator = set_t::const_iterator;
    using iterator = set_t::iterator;

private:
    set_t _map;
    int _cacheValidity { 0 }; // incremented on mempool change
public:
    auto cache_validity() const { return _cacheValidity; }

    auto size() const { return _map.size(); }
    auto& operator()()
    {
        _cacheValidity += 1;
        return _map;
    }
    auto& operator()() const { return _map; }
    [[nodiscard]] std::vector<const_iterator> by_fee_inc(AccountId) const;
};

struct ByFeeDesc {
    using const_iter_t = Txset::const_iterator;
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
