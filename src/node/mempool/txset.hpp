#pragma once

#include "block/body/transaction_id.hpp"
#include "entry.hpp"
#include <set>

class HashView;
namespace mempool {
struct ComparatorTransactionId {
    using is_transparent = std::true_type;
    bool operator()(const Entry& m1, const AccountId& accId2) const
    {
        return m1.txid().accountId < accId2;
    }
    bool operator()(const AccountId& accId1, const Entry& m2) const
    {
        return accId1 < m2.txid().accountId;
    }
    bool operator()(const TransactionId& txid1, const Entry& m2) const
    {
        return txid1 < m2.txid();
    }
    bool operator()(const Entry& m1, const TransactionId& txid2) const
    {
        return m1.txid() < txid2;
    }
    bool operator()(const Entry& m1, const Entry& m2) const
    {
        return m1.txid() < m2.txid();
    }
};
class Txset {
public:
    using set_t = std::set<Entry, ComparatorTransactionId>;
    using const_iter_t = set_t::const_iterator;
    using iterator = set_t::iterator;

private:
    set_t _set;
    int _cacheValidity { 0 }; // incremented on mempool change
public:
    auto cache_validity() const { return _cacheValidity; }

    auto size() const { return _set.size(); }
    auto& operator()()
    {
        _cacheValidity += 1;
        return _set;
    }
    auto& operator()() const { return _set; }
    [[nodiscard]] std::vector<const_iter_t> by_fee_inc_le(AccountId, wrt::optional<CompactUInt> threshold = {}) const;
};

class TokenData {
    using const_iterator_t = Txset::const_iter_t;
    struct Entry {
        std::vector<const_iterator_t> iterators;
    };

public:
    bool insert(const_iterator_t iter)
    {
        _size += 1;
        // auto tokenId { iter->altToken };
        // if (tokenId == TokenId::WART)
        //     return;
        map[iter->altTokenId].iterators.push_back(iter);
        return true;
    }

    size_t erase(const_iterator_t iter)
    {
        auto tokenId { iter->altTokenId };
        // if (tokenId == TokenId::WART) {
        //     _size -= 1;
        //     return 1;
        // }
        if (auto it { map.find(tokenId) }; it != map.end()) {
            auto n { std::erase(it->second.iterators, iter) };
            if (n != 0) { // deleted
                if (it->second.iterators.size() == 0)
                    map.erase(it);
                _size -= 1;
                return n;
            }
        }
        return 0;
    }

    auto size() const { return _size; }

private:
    size_t _size;
    std::map<TokenId, Entry> map;
};
namespace extractors {
using txit_t = Txset::const_iter_t;

template <typename T>
struct For {
    static_assert(false, "unsupported type to extract");
};

template <>
struct For<PinHeight> {
    [[nodiscard]] auto operator()(Txset::const_iter_t it) const { return it->pin_height(); };
};

template <>
struct For<TransactionId> {
    [[nodiscard]] auto operator()(Txset::const_iter_t it) const { return it->txid(); };
};

template <>
struct For<AccountId> {
    auto operator()(Txset::const_iter_t it) const { return it->from_id(); }
    auto operator()(const AccountToken& at) const { return at.account_id(); }
};

template <>
struct For<TokenId> {
    auto operator()(Txset::const_iter_t it) const { return it->altTokenId; }
    auto operator()(const AccountToken& at) const { return at.token_id(); }
};

template <>
struct For<CompactUInt> {
    auto operator()(Txset::const_iter_t it) const { return it->compact_fee(); };
};

template <>
struct For<TxHeight> {
    auto operator()(Txset::const_iter_t it) const { return it->txHeight; };
};

template <>
struct For<NonceId> {
    auto operator()(Txset::const_iter_t it) const { return it->nonce_id(); }
};

template <typename T>
struct GetExtractor {
    static constexpr struct : public For<T> {
        using For<T>::operator();
        const T& operator()(const T& t) const { return t; }
    } value {};
};

}

struct ByFeeDesc {
    using const_iter_t = Txset::const_iter_t;
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
