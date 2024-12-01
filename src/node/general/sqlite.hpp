#pragma once
#include "SQLiteCpp/SQLiteCpp.h"
#include "block/body/account_id.hpp"
#include "block/body/container.hpp"
#include "block/chain/height.hpp"
#include "block/chain/history/index.hpp"
#include "block/header/header.hpp"
#include "block/id.hpp"
#include "crypto/address.hpp"
#include "crypto/hash.hpp"
#include "general/reader.hpp"
#include "general/timestamp.hpp"
#include "sqlite_fwd.hpp"

namespace sqlite {
class RunningStatement {
    friend class Statement;

public:
    class Sentinel { };
    class Iterator {
        friend class RunningStatement;

    public:
        const Row& operator*()
        {
            return r;
        }
        bool operator==(const Sentinel&) const
        {
            return r.has_value();
        }
        Iterator& operator++()
        {
            r = Row(s.stmt);
            return *this;
        }

    private:
        Iterator(RunningStatement& s)
            : s(s)
            , r(s.stmt)
        {
        }
        RunningStatement& s;
        Row r;
    };
    Iterator begin() { return { *this }; };
    Sentinel end() { return {}; }

    ~RunningStatement()
    {
        stmt.reset();
    }

private:
    RunningStatement(SQLite::Statement& stmt);
    Statement& stmt;
};
}
namespace sqlite {
inline Column::operator Hash() const
{
    return { get_array<32>() };
}
inline Column::operator Height() const
{
    int64_t h = getInt64();
    if (h < 0) {
        throw std::runtime_error("Database corrupted. Negative height h="
            + std::to_string(h) + " observed");
    }
    return Height(h);
}
inline Column::operator HistoryId() const
{
    int64_t h = getInt64();
    if (h <= 0) {
        throw std::runtime_error("Database corrupted. HistoryId not positive.");
    }
    return HistoryId { uint64_t(h) };
}
template <size_t size>
std::array<uint8_t, size> Column::get_array() const
{
    std::array<uint8_t, size> res;
    if (getBytes() != size)
        throw std::runtime_error(
            "Database corrupted, cannot load " + std::to_string(size) + " bytes");
    memcpy(res.data(), getBlob(), size);
    return res;
}

inline std::vector<uint8_t> Column::get_vector() const
{
    std::vector<uint8_t> res(getBytes());
    memcpy(res.data(), getBlob(), getBytes());
    return res;
}
inline Column::operator std::vector<uint8_t>() const
{
    return get_vector();
}
inline Column::operator Address() const
{
    return get_array<20>();
}
inline Column::operator BodyContainer() const
{
    return { std::vector<uint8_t>(*this) };
}
inline Column::operator Header() const
{
    return get_array<80>();
    ;
}
inline Column::operator NonzeroHeight() const
{
    Height h { *this };
    return h.nonzero_throw("Database corrupted, block has height 0");
}
inline Column::operator int64_t() const
{
    return getInt64();
}
inline Column::operator AccountId() const
{
    return AccountId(int64_t(getInt64()));
}
inline Column::operator IsUint64() const
{
    return IsUint64(int64_t(getInt64()));
}
inline Column::operator BlockId() const
{
    return BlockId(getInt64());
}
inline Column::operator Funds() const
{
    auto v { Funds::from_value(int64_t(getInt64())) };
    if (!v.has_value())
        throw std::runtime_error("Database corrupted, invalid funds");
    return *v;
}
inline Column::operator uint64_t() const
{
    auto i { getInt64() };
    if (i < 0)
        throw std::runtime_error("Database corrupted, expected nonnegative entry");
    return (uint64_t)i;
}
inline Statement& Row::statement() const
{
    return st.get();
}
inline Column Row::operator[](int index) const
{
    value_assert();
    return statement().getColumn(index);
}

template <typename T>
inline T Row::get(int index)
{
    return operator[](index);
}

template <size_t N>
inline std::array<uint8_t, N> Row::get_array(int index) const
{
    value_assert();
    return statement().getColumn(index);
}

inline std::vector<uint8_t> Row::get_vector(int index) const
{
    value_assert();
    return statement().getColumn(index);
}

template <typename T>
inline Row::operator std::optional<T>()
{
    if (!hasValue)
        return {};
    return get<T>(0);
}

inline auto Row::process(auto lambda) const
{
    using ret_t = std::remove_cvref_t<decltype(lambda(*this))>;
    std::optional<ret_t> r;
    if (has_value())
        r = lambda(*this);
    return r;
}

inline void Row::value_assert() const
{
    if (!hasValue) {
        throw std::runtime_error(
            "Database error: trying to access empty result.");
    }
}
inline Row::Row(Statement& st)
    : st(st)
{
    hasValue = statement().executeStep();
}

inline Column Statement::getColumn(const int aIndex)
{
    return { SQLite::Statement::getColumn(aIndex) };
}
inline void Statement::bind(const int index, const Worksum& ws)
{
    bind(index, ws.to_bytes());
};
inline void Statement::bind(const int index, const std::vector<uint8_t>& v)
{
    SQLite::Statement::bind(index, v.data(), v.size());
}
template <size_t N>
inline void Statement::bind(const int index, std::array<uint8_t, N> a)
{
    SQLite::Statement::bind(index, a.data(), a.size());
}
template <size_t N>
inline void Statement::bind(const int index, View<N> v)
{
    SQLite::Statement::bind(index, v.data(), v.size());
}
inline void Statement::bind(const int index, Funds f)
{
    SQLite::Statement::bind(index, (int64_t)f.E8());
}
inline void Statement::bind(const int index, int64_t id)
{
    SQLite::Statement::bind(index, (int64_t)id);
}
inline void Statement::bind(const int index, IsUint64 id)
{
    SQLite::Statement::bind(index, (int64_t)id.value());
}
inline void Statement::bind(const int index, BlockId id)
{
    SQLite::Statement::bind(index, (int64_t)id.value());
}
inline void Statement::bind(const int index, Height id)
{
    SQLite::Statement::bind(index, (int64_t)id.value());
}
inline void Statement::bind(const int index, const std::string& s)
{
    SQLite::Statement::bindNoCopy(index, s.data(), s.size());
}
inline void Statement::bind(const int index, Timestamp ts){
    bind(index,ts.val());
}
template <size_t i>
void Statement::recursive_bind()
{
}
template <size_t i, typename T, typename... Types>
void Statement::recursive_bind(T&& t, Types&&... types)
{
    bind(i, std::forward<T>(t));
    recursive_bind<i + 1>(std::forward<Types>(types)...);
}
template <typename... Types>
inline uint32_t Statement::run(Types&&... types)
{
    recursive_bind<1>(std::forward<Types>(types)...);
    auto nchanged = exec();
    reset();
    assert(nchanged >= 0);
    return nchanged;
}

template <typename... Types, typename Lambda>
void Statement::for_each(Lambda lambda, Types&&... types)
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

template <typename... Types>
auto Statement::loop(Types&&... types)
{
    recursive_bind<1>(std::forward<Types>(types)...);
    return RunningStatement { *this };
}

}

// }
