#pragma once
#include "block/body/account_id.hpp"
#include "chainserver/db/chain_db.hpp"
#include "chainserver/db/types.hpp"
#include "communication/message_elements/byte_size.hpp"
#include "general/funds.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"

struct IdBalance {
    BalanceId id;
    Funds_uint64 balance;
    IdBalance(BalanceId id, Funds_uint64 balance)
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
        return { r };
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
    friend Writer& operator<<(Writer& w, const Serializable<>&) { return w; }
    Serializable() { }
    Serializable(Reader&) { }
};

template <typename T1, typename... T>
class Serializable<T1, T...> {
    using rest_t = Serializable<T...>;
    T1 _value;
    rest_t _rest;

public:
    void serialize(Serializer auto&& s)
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
        out.reserve(byte_size());
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

using BaseData = Serializable<StateId32, StateId64, std::vector<IdBalance>, std::vector<OrderData>, std::vector<OrderFillstate>>;
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
        : BaseData(db.next_id32(), db.next_id64(), {}, {}, {})
    {
    }
    auto& next_state_id32() const { return get<0>(); }
    auto& next_state_id64() const { return get<1>(); }
    auto& original_balances() { return get<2>(); }
    auto& original_balances() const { return get<2>(); }
    auto& original_orders() { return get<3>(); }
    auto& original_orders() const { return get<3>(); }
    auto& original_fillstates() { return get<4>(); }
    auto& original_fillstates() const { return get<4>(); }

private:
    void register_object(auto& vector, auto element)
    {
        if (element.id >= next_state_id64())
            return;

        bool exists = false;
        for (auto& b : vector) {
            if (b.id == element.id)
                exists = true;
            break;
        }
        assert(!exists);
        vector.push_back(std::move(element));
    }

public:
    void register_original_balance(IdBalance o)
    {
        register_object(original_balances(), std::move(o));
    }
    void register_original_order(chain_db::OrderData o)
    {
        register_object(original_orders(), std::move(o));
    };
    void register_original_fillstate(chain_db::OrderFillstate o)
    {
        register_object(original_fillstates(), std::move(o));
    }

    void foreach_balance_update(const auto& lambda) const
    {
        for (auto& o : original_balances())
            lambda(o);
    }
};
}
