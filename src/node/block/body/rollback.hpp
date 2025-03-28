#pragma once
#include "block/body/account_id.hpp"
#include "chainserver/db/chain_db.hpp"
#include "communication/message_elements/byte_size.hpp"
#include "defi/token/id.hpp"
#include "general/funds.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"

struct BalanceIdFunds {
    BalanceId balanceId;
    Funds_uint64 funds;
    BalanceIdFunds(BalanceId balanceId, Funds_uint64 funds)
        : balanceId(balanceId)
        , funds(funds)
    {
    }
    BalanceIdFunds(Reader&);
};

namespace rollback {
template <typename T>
struct TypeSerializer {
    static T read(Reader& r)
    {
        return { r };
    }
    static void write(const T& v, Writer& w)
    {
        w << v;
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
    static void write(const std::vector<T>& v, Writer& w)
    {
        w << uint32_t(v.size());
        for (auto& e : v)
            w << e;
    }
};

template <typename T>
void write_type(const T& t, Writer& w)
{
    TypeSerializer<T>::write(t, w);
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

protected:
    friend Writer& operator<<(Writer& w, const Serializable<T1, T...>& s)
    {
        write_type(s._value, w);
        return w << s._rest;
    }

public:
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
        return ::byte_size(_value) + rest_t::byte_size();
    }
};

using BaseData = Serializable<AccountId, TokenId, uint64_t, std::vector<BalanceIdFunds>>;
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
        : BaseData(db.next_account_id(), db.next_token_id(), db.next_state_id().value(), {})
    {
    }
    auto& next_account_id() const { return get<0>(); }
    auto& next_token_id() const { return get<1>(); }
    auto& next_state_id() const { return get<2>(); }
    auto& original_balances() { return get<3>(); }
    auto& original_balances() const { return get<3>(); }
    void register_balance(BalanceId balanceId, Funds_uint64 originalBalance)
    {
        if (balanceId.value() >= next_state_id())
            return;

        auto& ob { original_balances() };
        bool exists = false;
        for (auto& b : ob) {
            if (b.balanceId == balanceId)
                exists = true;
            break;
        }
        assert(!exists);
        ob.push_back({ balanceId, originalBalance });
    }

    void foreach_balance_update(const auto& lambda) const
    {
        for (auto& o : original_balances())
            lambda(o);
    }
};
}
