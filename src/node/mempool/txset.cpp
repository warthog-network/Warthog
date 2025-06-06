#include "txset.hpp"
#include <algorithm>
#include <random>
namespace mempool {
auto Txset::by_fee_inc(AccountId id) const -> std::vector<const_iterator>
{
    auto lb { _map.lower_bound(id) };
    auto ub { _map.upper_bound(id) };
    std::vector<const_iterator> iterators;
    for (auto iter { lb }; iter != ub; ++iter)
        iterators.push_back(iter);
    std::sort(iterators.begin(), iterators.end(), [](auto iter1, auto iter2) {
        return iter1->second.fee < iter2->second.fee;
    });
    return iterators;
};

bool ByFeeDesc::insert(const_iter_t iter)
{
    auto pos = std::lower_bound(data.begin(), data.end(), iter, [](const_iter_t i1, const_iter_t i2) { return i1->second.fee > i2->second.fee; });
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
