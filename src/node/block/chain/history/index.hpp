#pragma once
#include "general/with_uint64.hpp"
struct HistoryId : public IsUint64 {
    using IsUint64::IsUint64;
    bool operator==(const HistoryId&) const = default;
    size_t operator-(HistoryId a)
    {
        return val - a.val;
    }
    HistoryId operator-(size_t i) const{
        return HistoryId(val - i);
    }
    HistoryId operator+(size_t i) const
    {
        return HistoryId(val + i);
    }
    HistoryId operator++(int)
    {
        return HistoryId(val++);
    }
    HistoryId operator++()
    {
        return HistoryId(++val);
    }
};
