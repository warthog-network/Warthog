#include "txmap.hpp"
#include <algorithm>
#include <random>
#include <ranges>
namespace mempool {
auto Txmap::by_fee_inc(AccountId id) -> std::vector<iterator>
{
    auto ar { account_range(id) };
    std::vector<decltype(ar.begin())> iterators;
    for (auto iter { ar.begin() }; iter != ar.end(); ++iter)
        iterators.push_back(iter);
    std::sort(iterators.begin(), iterators.end(), [](auto iter1, auto iter2) {
        return iter1->second.fee < iter2->second.fee;
    });
    return iterators;
};

void ByFee::insert(iter_t iter)
{
    auto pos = std::lower_bound(data.begin(), data.end(), iter, [](iter_t i1, iter_t i2) { return i1->second.fee > i2->second.fee; });
    data.insert(pos, iter);
}

size_t ByFee::erase(iter_t iter)
{
    return std::erase(data, iter);
}

auto ByFee::sample(size_t n, size_t k) const -> std::vector<iter_t>
{
    n = std::min(n, data.size());
    k = std::min(n, k);

    std::vector<iter_t> res;
    std::sample(data.begin(), data.begin() + n, std::back_inserter(res), k,
        std::mt19937 { std::random_device {}() });
    return res;
}

}
