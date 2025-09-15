#pragma once
#include "block/body/account_id.hpp"
#include "defi/token/id.hpp"
#include "general/with_uint64.hpp"
class CancelId : public UInt64WithOperators<CancelId> { };

// This state id will probably not exceed 2^32.
// We use it as joint id for slow-growing tables like accounts
// and assets. By doing this, the sqlite table will not need as much space for the
// table ids as it would if we used the same globally incremented id over
// all tables.
class StateId32 : public UInt64WithOperators<StateId32> {
public:
    StateId32(AccountId aid)
        : UInt64WithOperators<StateId32>(aid.value()) { };
    StateId32(AssetId aid)
        : UInt64WithOperators<StateId32>(uint64_t(aid.value())) { };
    using parent_t::parent_t;
};

// This state id can grow over 2^32 in the long run.
class StateId64 : public UInt64WithOperators<StateId64> {
public:
    StateId64(BalanceId id)
        : UInt64WithOperators<StateId64>(id.value()) { };
    using parent_t::parent_t;
};
