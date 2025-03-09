#pragma once
#include "SQLiteCpp/SQLiteCpp.h"
#include "api/types/forward_declarations.hpp"
#include "block/block.hpp"
#include "block/body/order_id.hpp"
#include "block/chain/offsts.hpp"
#include "block/chain/worksum.hpp"
#include "block/id.hpp"
#include "chainserver/transaction_ids.hpp"
#include "crypto/address.hpp"
#include "defi/uint64/price.hpp"
#include "defi/token/token.hpp"
#include "general/filelock/filelock.hpp"
struct Column2 : public SQLite::Column {

    int64_t getInt64() const noexcept
    {
        return SQLite::Column::getInt64();
    }

    uint64_t getUInt64() const
    {
        auto i { SQLite::Column::getInt64() };
        if (i < 0) {
            throw std::runtime_error("Database might be corrupted. Expected non-negative value.");
        }
        return i;
    }

    uint64_t getUInt32() const
    {
        auto i { getUInt64() };
        if (i > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("Database might be corrupted. Value overflows uint32_t.");
        return i;
    }

    operator Hash() const
    {
        return { get_array<32>() };
    }
    operator TokenHash() const
    {
        return static_cast<Hash>(*this);
    }
    operator Height() const
    {
        return Height(getUInt32());
    }
    operator HistoryId() const
    {
        return HistoryId { getUInt64() };
    }
    template <size_t size>
    std::array<uint8_t, size> get_array() const
    {
        std::array<uint8_t, size> res;
        if (getBytes() != size)
            throw std::runtime_error(
                "Database corrupted, cannot load " + std::to_string(size) + " bytes");
        memcpy(res.data(), getBlob(), size);
        return res;
    }

    std::vector<uint8_t> get_vector() const
    {
        std::vector<uint8_t> res(getBytes());
        memcpy(res.data(), getBlob(), getBytes());
        return res;
    }
    operator std::vector<uint8_t>() const
    {
        return get_vector();
    }
    operator Address() const
    {
        return get_array<20>();
    }
    operator TokenName()
    {
        return TokenName::parse_throw(static_cast<std::string>(*this));
    }
    operator BodyContainer() const
    {
        return { std::vector<uint8_t>(*this) };
    }
    operator Header() const
    {
        return get_array<80>();
        ;
    }
    operator NonzeroHeight() const
    {
        Height h { *this };
        return h.nonzero_throw("Database corrupted, block has height 0");
    }
    operator int64_t() const
    {
        return getInt64();
    }
    operator Price_uint64() const
    {
        auto p { Price_uint64::from_uint32(getUInt32()) };
        if (!p)
            throw std::runtime_error("Cannot parse price");
        return *p;
    }
    operator OrderId() const
    {
        return OrderId(getInt64());
    }
    operator AccountId() const
    {
        return AccountId(getInt64());
    }
    operator TokenId() const
    {
        return TokenId(getInt64());
    }
    operator IsUint64() const
    {
        return IsUint64(getInt64());
    }
    operator BlockId() const
    {
        return BlockId(getInt64());
    }
    operator Funds_uint64() const
    {
        return Funds_uint64(getUInt());
    }
    operator Funds() const
    {
        auto v { Funds::from_value(int64_t(getInt64())) };
        if (!v.has_value())
            throw std::runtime_error("Database corrupted, invalid funds");
        return *v;
    }
    operator uint64_t() const
    {
        auto i { getInt64() };
        if (i < 0)
            throw std::runtime_error("Database corrupted, expected nonnegative entry");
        return (uint64_t)i;
    }
    template <typename T>
    operator std::optional<T>() const
    {
        if (isNull())
            return {};
        return static_cast<T>(*this);
    }
};
struct Statement2 : public SQLite::Statement {
    using SQLite::Statement::Statement;

    using SQLite::Statement::bind;
    Column2 getColumn(const int aIndex)
    {
        return { Statement::getColumn(aIndex) };
    }

    void bind(const int index, std::span<uint8_t> s)
    {
        SQLite::Statement::bind(index, s.data(), s.size());
    }

    auto bind_convert(const std::vector<uint8_t>& v) { return std::span(v); }
    auto bind_convert(const Worksum& ws) { return ws.to_bytes(); };
    template <size_t N>
    auto bind_convert(std::array<uint8_t, N> a) { return std::span<uint8_t>(a); }

    template <size_t N>
    auto bind_convert(View<N> v) { return v.span(); }
    auto bind_convert(Funds f) { return (int64_t)f.E8(); };
    auto bind_convert(uint64_t id)
    {
        assert(id < std::numeric_limits<uint64_t>::max());
        return (int64_t)id;
    };
    auto bind_convert(IsUint64 id) { return bind_convert(id.value()); };
    auto bind_convert(IsUint32 id) { return bind_convert(id.value()); };
    auto bind_convert(Price_uint64 p) { return p.to_uint32(); };

    template <typename T>
    void bind_to(const int index, T&& t)
    {
        bind(index, bind_convert(std::forward<T>(t)));
    }

    template <size_t i>
    void recursive_bind()
    {
    }
    template <size_t i, typename T, typename... Types>
    void recursive_bind(T&& t, Types&&... types)
    {
        bind(i, std::forward<T>(t));
        recursive_bind<i + 1>(std::forward<Types>(types)...);
    }
    template <typename... Types>
    auto& bind_multiple(Types&&... types)
    {
        recursive_bind<1>(std::forward<Types>(types)...);
        return *this;
    }

    template <typename... Types>
    uint32_t run(Types&&... types)
    {
        bind_multiple(std::forward<Types>(types)...);
        auto nchanged = exec();
        reset();
        assert(nchanged >= 0);
        return nchanged;
    }

    // private:
    struct Row {
        friend class OrderLoader;
        Column2 operator[](int index) const
        {
            value_assert();
            return st.getColumn(index);
        }

        template <typename T>
        T get(int index)
        {
            return operator[](index);
        }

        template <size_t N>
        std::array<uint8_t, N> get_array(int index) const
        {
            value_assert();
            return st.getColumn(index);
        }
        std::vector<uint8_t> get_vector(int index) const
        {
            value_assert();
            return st.getColumn(index);
        }

        template <typename T>
        operator std::optional<T>()
        {
            if (!hasValue)
                return {};
            return get<T>(0);
        }
        bool has_value() const { return hasValue; }
        auto process(auto lambda) const
        {
            using ret_t = std::remove_cvref_t<decltype(lambda(*this))>;
            std::optional<ret_t> r;
            if (has_value())
                r = lambda(*this);
            return r;
        }

    private:
        void value_assert() const
        {
            if (!hasValue) {
                throw std::runtime_error(
                    "Database error: trying to access empty result.");
            }
        }
        friend struct Statement2;
        Row(Statement2& st)
            : st(st)
        {
            hasValue = st.executeStep();
        }
        Statement2& st;
        bool hasValue;
    };
    struct SingleResult : public Row {
        using Row::Row;
        ~SingleResult()
        {
            if (hasValue)
                assert(st.executeStep() == false);
            st.reset();
        }
    };

public:
    template <typename... Types>
    [[nodiscard]] SingleResult one(Types&&... types)
    {
        recursive_bind<1>(std::forward<Types>(types)...);
        return SingleResult { *this };
    }

    template <typename... Types, typename Lambda>
    void for_each(Lambda lambda, Types&&... types)
    {
        recursive_bind<1>(std::forward<Types>(types)...);
        while (true) {
            auto r { Row(*this) };
            if (!r.has_value())
                break;
            lambda(r);
        }
        reset();
    }
};
