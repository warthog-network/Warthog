#pragma once
#include "block/body/account_id.hpp"
#include "defi/token/id.hpp"
#include "general/with_uint64.hpp"
class CancelId : public UInt64WithOperators<CancelId> { };

// The Ids describe the types that are managed by this state type
template <typename self_t, typename... Ids>
struct StateIdBase : public UInt64WithOperators<self_t> {
    using parent_t = StateIdBase;
    using UInt64WithOperators<self_t>::UInt64WithOperators;
    template <typename T>
    requires(std::is_same_v<T, Ids> || ...)
    StateIdBase(T t)
        : UInt64WithOperators<self_t>(t.value())
    {
    }
    template <typename T>
    requires(std::is_same_v<T, Ids> || ...)
    operator T() const
    {
        return T(this->value());
    }
    [[nodiscard]] static self_t max_component(auto& generator)
    {
        self_t v(0);
        ([&](auto&& id) {
            if (uint64_t(v.value()) < uint64_t(id.value()))
                v = self_t(uint64_t(id.value()));
        }(static_cast<Ids>(generator)),
            ...);
        return v;
    }
};

// This state id will probably not exceed 2^32.
// We use it as joint id for slow-growing tables like accounts
// and assets. By doing this, the sqlite table will not need as much space for the
// table ids as it would if we used the same globally incremented id over
// all tables.
class StateId32 : public StateIdBase<StateId32, AccountId, AssetId> {
    using parent_t::parent_t;
};

// This state id can grow over 2^32 in the long run.
class StateId64 : public StateIdBase<StateId64, BalanceId, TokenForkBalanceId> {
public:
    using parent_t::parent_t;
};
