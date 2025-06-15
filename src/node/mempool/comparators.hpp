#pragma once
#include "block/chain/height.hpp"
#include "txset.hpp"
#include <type_traits>

namespace mempool {

struct ComparatorPin {
    using const_iter_t = Txset::const_iterator;
    using is_transparent = std::true_type;
    inline bool operator()(const_iter_t i1, Height h2) const
    {
        return (i1->pin_height() < h2);
    }
    inline bool operator()(const_iter_t i1, const_iter_t i2) const
    {
        if (i1->pin_height() == i2->pin_height())
            return i1->txid() < i2->txid();
        return (i1->pin_height() < i2->pin_height());
    }
};
struct ComparatorFee {
    using const_iter_t = Txset::const_iterator;
    inline bool operator()(const_iter_t i1, const_iter_t i2) const
    {
        if (i1->compact_fee() == i2->compact_fee()) {
            return i1->txid() < i2->txid();
        };
        return i1->compact_fee() > i2->compact_fee();
    }
};
struct ComparatorHash {
    using const_iter_t = Txset::const_iterator;
    using is_transparent = std::true_type;
    inline bool operator()(const_iter_t i1, const_iter_t i2) const
    {
        return i1->txhash < i2->txhash;
    }
    inline bool operator()(const_iter_t i1, const HashView rhs) const
    {
        return i1->txhash < rhs;
    }
    inline bool operator()(HashView lhs, const_iter_t i2) const
    {
        return lhs < i2->txhash;
    }
};
}
