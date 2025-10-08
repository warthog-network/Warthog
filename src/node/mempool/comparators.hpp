#pragma once
#include "block/chain/height.hpp"
#include "defi/token/account_token.hpp"
#include "general/lex_compare.hpp"
#include "txset.hpp"
#include <type_traits>

namespace mempool {

struct ComparatorPin {
    using const_iter_t = Txset::const_iter_t;
    using is_transparent = std::true_type;
    inline bool operator()(const_iter_t i1, Height h2) const
    {
        return (i1->pin_height() < h2);
    }
    inline bool operator()(const_iter_t it1, const_iter_t it2) const
    {
        return lex_compare_less_by(it1, it2, get_pin_height, get_txid);
    }
};

struct ComparatorHash {
    using const_iter_t = Txset::const_iter_t;
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
struct ComparatorAccountFee {
    using const_iter_t = Txset::const_iter_t;
    using is_transparent = std::true_type;
    bool operator()(const_iter_t it1, const_iter_t it2) const
    {
        return lex_compare_less_by(it1, it2, get_account_id, get_fee, get_nonce_id);
    }
    inline bool operator()(const_iter_t i1, const AccountId& rhs) const
    {
        return i1->from_id() < rhs;
    }
    inline bool operator()(const AccountId& lhs, const_iter_t i2) const
    {
        return lhs < i2->from_id();
    }
};
struct ComparatorAccountTokenFee {
    using const_iter_t = Txset::const_iter_t;
    using is_transparent = std::true_type;
    bool operator()(const_iter_t it1, const_iter_t it2) const
    {
        return lex_compare_less_by(it1, it2, get_account_id, get_token_id, get_fee, get_nonce_id);
    }
    inline bool operator()(const_iter_t i1, const AccountToken& rhs) const
    {
        return AccountToken(i1->from_id(), i1->altToken) < rhs;
    }
    inline bool operator()(const AccountToken& lhs, const_iter_t i2) const
    {
        return lhs < AccountToken(i2->from_id(), i2->altToken);
    }
};
}
