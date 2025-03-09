#pragma once
#include "general/with_uint64.hpp"
class HistoryId : public IsUint64 {
public:
    using IsUint64::IsUint64;
    static HistoryId smallest() { return HistoryId(1); }
    bool operator==(const HistoryId&) const = default;
    size_t operator-(HistoryId a)
    {
        return val - a.val;
    }
    HistoryId operator-(size_t i) const
    {
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
