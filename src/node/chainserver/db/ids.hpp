#pragma once
#include "general/with_uint64.hpp"
class CancelId : public UInt64WithOperators<CancelId> { };
class StateId : public UInt64WithOperators<StateId> {
    using parent_t::parent_t;
};
