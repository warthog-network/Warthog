#pragma once
#include "block/body/account_id.hpp"
#include "chainserver/db/chain_db.hpp"
#include "chainserver/db/types.hpp"
#include "general/funds.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
#include "serialization/byte_size.hpp"

struct IdBalance {
    BalanceId id;
    Balance_uint64 balance;
    IdBalance(BalanceId id, Balance_uint64 balance)
        : id(id)
        , balance(balance)
    {
    }
    static size_t byte_size() { return BalanceId::byte_size() + Funds_uint64::byte_size(); }
    void serialize(Serializer auto&& s) const
    {
        s << id << balance;
    }
    IdBalance(Reader&);
};

namespace rollback {
template <typename T>
struct TypeSerializer {
    static T read(Reader& r)
    {
        return T(r);
    }
    static void write(const T& v, Serializer auto&& s)
    {
        s << v;
    }
};

template <typename T>
struct TypeSerializer<std::vector<T>> {
    static std::vector<T> read(Reader& r)
    {
        std::vector<T> v;
        const uint64_t l { uint32_t(r) };
        v.reserve(l);
        for (size_t i { 0 }; i < l; ++i)
            v.push_back(T(r));
        return v;
    }
    static void write(const std::vector<T>& v, Serializer auto&& s)
    {
        s << uint32_t(v.size());
        for (auto& e : v)
            s << e;
    }
};

template <typename T>
void write_type(const T& t, Serializer auto&& s)
{
    TypeSerializer<T>::write(t, s);
}

template <typename T>
auto read_type(Reader& r)
{
    return TypeSerializer<T>::read(r);
}
template <typename... T>
class Serializable;

template <>
class Serializable<> {
public:
    static constexpr size_t elements = 0;
    size_t byte_size() const { return 0; }
    void serialize(Serializer auto&&) const { } // do nothing
    Serializable() { }
    Serializable(Reader&) { }
};

template <typename T1, typename... T>
class Serializable<T1, T...> {
    using rest_t = Serializable<T...>;
    T1 _value;
    rest_t _rest;

public:
    void serialize(Serializer auto&& s) const
    {
        write_type(_value, s);
        s << _rest;
    }

    static constexpr size_t elements = rest_t::elements + 1;
    Serializable(Reader& r)
        : _value(read_type<T1>(r))
        , _rest(r)
    {
    }
    Serializable(T1 arg, T... args)
        : _value(std::move(arg))
        , _rest(std::move(args)...)
    {
    }

    template <size_t i>
    requires(i <= rest_t::elements && i != 0)
    auto& get()
    {
        return _rest.template get<i - 1>();
    }
    template <size_t i>
    requires(i <= rest_t::elements && i != 0)
    auto& get() const
    {
        return _rest.template get<i - 1>();
    }

    template <size_t i>
    requires(i == 0)
    auto& get()
    {
        return _value;
    }

    template <size_t i>
    requires(i == 0)
    auto& get() const
    {
        return _value;
    }
    std::vector<uint8_t> serialize()
    {
        std::vector<uint8_t> out;
        out.resize(byte_size());
        Writer w(out);
        w << *this;
        return out;
    }
    size_t byte_size() const
    {
        return ::byte_size(_value) + _rest.byte_size();
    }
};

struct OrderData : public chain_db::OrderData {
    OrderData(chain_db::OrderData od)
        : chain_db::OrderData(std::move(od))
    {
    }
    OrderData(Reader& r)
        : chain_db::OrderData { r, r, r, r, r, r, r }
    {
    }
};

struct OrderFillstate : public chain_db::OrderFillstate {
    OrderFillstate(chain_db::OrderFillstate fs)
        : chain_db::OrderFillstate(std::move(fs))
    {
    }
    OrderFillstate(Reader& r)
        : chain_db::OrderFillstate { r, r, r }
    {
    }
};
struct Poolstate {
    AssetId id;
    Funds_uint64 base;
    Funds_uint64 quote;
    Funds_uint64 shares;
    Poolstate(AssetId id, const defi::Pool_uint64& pool)
        : id(id)
        , base(pool.base)
        , quote(pool.quote)
        , shares(pool.shares_total())
    {
    }
    Poolstate(Reader& r)
        : id(r)
        , base(r)
        , quote(r)
        , shares(r)
    {
    }
    static constexpr size_t byte_size() { return AssetId::byte_size() + 3 * Funds_uint64::byte_size(); }
    void serialize(Serializer auto&& s) const
    {
        s << id << base << quote << shares;
    }
};

using BaseData = Serializable<
    StateId64, // 0
    serialization::Vector32<IdBalance>, // 1
    serialization::Vector32<OrderData>, // 2
    serialization::Vector32<OrderFillstate>, // 3
    serialization::Vector32<Poolstate>, // 4
    serialization::Vector32<AssetId>>; // 5
class Data : protected BaseData {

private:
    Data(Reader v)
        : BaseData(v)
    {
    }

public:
    using BaseData::Serializable;
    using BaseData::serialize;
    Data(const std::vector<uint8_t>& v)
        : Data(Reader(v))
    {
    }
    Data(const ChainDB& db)
        : BaseData(db.next_id(), {}, {}, {}, {}, {})
    {
    }
    auto& next_state_id64() const { return get<0>(); }
    auto& original_balances() { return get<1>(); }
    auto& original_balances() const { return get<1>(); }
    auto& original_orders() { return get<2>(); }
    auto& original_orders() const { return get<2>(); }
    auto& original_fillstates() { return get<3>(); }
    auto& original_fillstates() const { return get<3>(); }
    auto& original_poolstates() { return get<4>(); }
    auto& original_poolstates() const { return get<4>(); }
    auto& newly_created_pools() { return get<5>(); }
    auto& newly_created_pools() const { return get<5>(); }

private:
    auto get_id(auto&& element)
    {
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(element)>, AssetId>)
            return element;
        else
            return element.id;
    }
    bool id_is_present(auto& vector, auto&& element)
    {
        for (auto& b : vector) {
            if (get_id(b) == get_id(element))
                return true;
        }
        return false;
    }
    void register_object(auto& vector, auto&& element)
    {
        assert(!id_is_present(vector, element));
        vector.push_back(std::move(element));
    }
    void register_object64(auto& vector, auto element)
    {
        if (StateId64::from_id(get_id(element)) >= next_state_id64())
            return;
        register_object(vector, std::move(element));
    }
    void register_object_hist(auto& vector, auto element, HistoryId nextHistoryId)
    {
        if (get_id(element) >= nextHistoryId)
            return;
        register_object(vector, std::move(element));
    }
    // void register_object32(auto& vector, auto element)
    // {
    //     if (state_id(get_id(element)) >= next_state_id32())
    //         return;
    //     register_object(vector, std::move(element));
    // }

public:
    void register_original_balance(IdBalance o)
    {
        register_object64(original_balances(), std::move(o));
    }
    void register_original_order(chain_db::OrderData o, HistoryId nextHistoryId)
    {
        register_object_hist(original_orders(), std::move(o), nextHistoryId);
    };
    void register_original_fillstate(chain_db::OrderFillstate o, HistoryId nextHistoryId)
    {
        register_object_hist(original_fillstates(), std::move(o), nextHistoryId);
    }
    void register_original_poolstate(Poolstate o)
    {
        register_object64(original_poolstates(), std::move(o));
    }
    void register_newly_created_pool(AssetId id)
    {
        register_object64(newly_created_pools(), std::move(id));
    }

    void foreach_changed_balance(const auto& lambda) const
    {
        for (auto& o : original_balances())
            lambda(o);
    }
    void foreach_deleted_order(const auto& lambda) const
    {
        for (auto& o : original_orders())
            lambda(o);
    }
    void foreach_changed_order(const auto& lambda) const
    {
        for (auto& o : original_fillstates())
            lambda(o);
    }
    void foreach_newly_created_pool(const auto& lambda) const
    {
        for (auto& o : newly_created_pools())
            lambda(o);
    }
    void foreach_changed_poolstate(const auto& lambda) const
    {
        for (auto& o : original_poolstates())
            lambda(o);
    }
};
}
