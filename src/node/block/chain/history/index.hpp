#pragma once
#include "general/with_uint64.hpp"
class HistoryId : public UInt64WithOperators<HistoryId> {
public:
    using parent_t::parent_t;
    static HistoryId smallest() { return HistoryId(1); }
};
