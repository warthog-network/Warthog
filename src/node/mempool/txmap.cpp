#include "txmap.hpp"
#include <algorithm>
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

}
