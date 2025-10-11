#include "txset.hpp"
#include "general/lex_compare.hpp"
#include <algorithm>
#include <random>
namespace mempool {
auto Txset::by_fee_inc_le(AccountId id, std::optional<CompactUInt> threshold) const -> std::vector<const_iter_t>
{
    auto lb { _set.lower_bound(id) };
    auto ub { _set.upper_bound(id) };
    std::vector<const_iter_t> iterators;
    for (auto iter { lb }; iter != ub; ++iter) {
        if (threshold && (iter->compact_fee() <= threshold)) // "le" in function name stands for less or equal
            iterators.push_back(iter);
    }
    std::sort(iterators.begin(), iterators.end(), [](const_iter_t it1, const_iter_t it2) {
        return lex_less(it1, it2, get_fee, get_nonce_id);
    });
    return iterators;
};

bool ByFeeDesc::insert(const_iter_t iter)
{
    auto pos = std::lower_bound(data.begin(), data.end(), iter, [](const_iter_t i1, const_iter_t i2) { return i1->compact_fee() > i2->compact_fee(); });
    if (pos != data.end() && *pos == iter)
        return false;
    data.insert(pos, iter);
    return true;
}

size_t ByFeeDesc::erase(const_iter_t iter)
{
    return std::erase(data, iter);
}

auto ByFeeDesc::sample(size_t n, size_t k) const -> std::vector<const_iter_t>
{
    n = std::min(n, data.size());
    k = std::min(n, k);

    std::vector<const_iter_t> res;
    std::sample(data.begin(), data.begin() + n, std::back_inserter(res), k,
        std::mt19937 { std::random_device {}() });
    return res;
}

}
