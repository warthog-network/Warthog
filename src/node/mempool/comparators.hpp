#pragma once
#include "block/chain/height.hpp"
#include "txmap.hpp"
#include <type_traits>

namespace mempool {
struct ComparatorPin {
    using iter_t = Txmap::iterator;
    using is_transparent = std::true_type;
    inline bool operator()(iter_t i1, Height h2) const
    {
        return (i1->first.pinHeight < h2);
    }
    inline bool operator()(iter_t i1, iter_t i2) const
    {
        if (i1->first.pinHeight == i2->first.pinHeight)
            return i1->first < i2->first;
        return (i1->first.pinHeight < i2->first.pinHeight);
    }
};
struct ComparatorFee {
    using iter_t = Txmap::iterator;
    inline bool operator()(iter_t i1, iter_t i2) const
    {
        if (i1->second.fee == i2->second.fee) {
            return i1->first < i2->first;
        };
        return i1->second.fee > i2->second.fee;
    }
};
struct ComparatorHash {
    using iter_t = Txmap::iterator;
    using is_transparent = std::true_type;
    inline bool operator()(iter_t i1, iter_t i2) const
    {
        return i1->second.hash < i2->second.hash;
    }
    inline bool operator()(iter_t i1, const HashView rhs) const
    {
        return i1->second.hash < rhs;
    }
    inline bool operator()(HashView lhs, iter_t i2) const
    {
        return lhs < i2->second.hash;
    }
};
}
