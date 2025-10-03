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
    static constexpr bool is_id_t() { return std::is_same_v<T, self_t> || (std::is_same_v<T, Ids> || ...); }

    // convierts from the Ids types
    template <typename T>
    requires(std::is_same_v<T, Ids> || ...)
    static self_t from_id(T t)
    {
        return self_t(uint64_t(t.value()));
    }
    template <typename T>
    requires(is_id_t<T>())
    operator T() const
    {
        if constexpr (std::is_same_v<self_t, T>)
            return *this;
        else
            return T(this->value());
    }
    template <typename T>
    requires(is_id_t<T>())
    void if_unequal_throw(T id) const
    {
        if (from_id(id) != *static_cast<const self_t*>(this))
            throw std::runtime_error("Internal error, token id inconsistent.");
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
class StateId32 : public StateIdBase<StateId32, AccountId> {
    using parent_t::parent_t;
};

// This state id can grow over 2^32 in the long run.
class StateId64 : public StateIdBase<StateId64, BalanceId, TokenForkBalanceId, AssetId> {
public:
    using parent_t::parent_t;
};

template <typename T>
[[nodiscard]] auto state_id(T&& t)
{
    if constexpr (StateId32::is_id_t<std::remove_cvref_t<T>>()) {
        return StateId32::from_id(t);
    } else if constexpr (StateId64::is_id_t<std::remove_cvref_t<T>>()) {
        return StateId64::from_id(t);
    } else {
        static_assert(false, "argument has no state id");
    }
}

class StateIncrementer {

    template <typename base_t>
    class NextBase {
    protected:
        template <typename T>
        static constexpr bool is_id_t()
        {
            return StateId32::is_id_t<T>() || StateId64::is_id_t<T>() || std::is_same_v<T, StateId32> || std::is_same_v<T, StateId64>;
        }
        base_t& s;
        NextBase(base_t& s)
            : s(s)
        {
        }

    public:
        operator StateId32() &&
        {
            return s.next32;
        }
        operator StateId64() &&
        {
            return s.next64;
        }
    };
    class Next : public NextBase<const StateIncrementer> {
        friend class StateIncrementer;
        using NextBase<const StateIncrementer>::NextBase;

    public:
        template <typename T>
        requires(is_id_t<T>())
        operator T() &&
        {
            return s.get_state<T>();
        }
    };
    class NextInc : public NextBase<StateIncrementer> {
        friend class StateIncrementer;
        using NextBase<StateIncrementer>::NextBase;

    public:
        template <typename T>
        requires(is_id_t<T>())
        operator T() &&
        {
            return s.get_state<T>()++;
        }
    };

public:
    StateId32 next32;
    StateId64 next64;

public:
    template <typename T>
    requires(StateId32::is_id_t<T>())
    auto& get_state()
    {
        return next32;
    }
    template <typename T>
    requires(StateId32::is_id_t<T>())
    auto& get_state() const
    {
        return next32;
    }
    template <typename T>
    requires(StateId64::is_id_t<T>())
    auto& get_state()
    {
        return next64;
    }
    template <typename T>
    requires(StateId64::is_id_t<T>())
    auto& get_state() const
    {
        return next64;
    }
    template <typename T>
    requires(StateId32::is_id_t<T>() || StateId64::is_id_t<T>())
    auto& corresponding_state(const T&)
    {
        return get_state<std::remove_cvref_t<T>>();
    }
    StateIncrementer(StateId32 next32, StateId64 next64)
        : next32(next32)
        , next64(next64)
    {
    }
    Next next() const { return { *this }; }
    NextInc next_inc() { return { *this }; }
};
