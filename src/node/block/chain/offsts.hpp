#pragma once
#include "block/body/account_id.hpp"
#include "block/chain/height.hpp"
#include "block/chain/history/index.hpp"
#include <algorithm>

template <typename HeightType, typename T>
struct Heights {
    Heights(std::vector<T> data = {})
        : data(std::move(data)) { };
    const T& at(NonzeroHeight h) const
    {
        return data.at((h - 1).value());
    }
    void shrink(Height newlength)
    {
        assert(size() >= newlength.value());
        data.erase(data.begin() + newlength.value(), data.end());
    }
    void append(T v)
    {
        data.push_back(std::move(v));
    }
    auto& last() const { return data.back(); }
    [[nodiscard]] HeightType height(const T& t) const
    {
        auto iter = std::upper_bound(data.begin(), data.end(),
            t);
        assert(iter != data.begin());
        return HeightType(uint32_t((iter - 1) - data.begin() + 1));
    }
    void append_vector(const std::vector<T>& v)
    {
        data.insert(data.end(), v.begin(), v.end());
    }
    size_t size() const
    {
        return data.size();
    }

protected:
    std::vector<T> data;
};
using HistoryHeights = Heights<NonzeroHeight, HistoryId>;
using AccountHeights = Heights<AccountHeight, AccountId>;
