#pragma once
#include "SQLiteCpp/SQLiteCpp.h"
#include "spdlog/spdlog.h"
#include "sqlite_fwd.hpp"
#include "type_conv.hpp"

namespace sqlite {
// class RunningStatement {
//     friend class Statement;
//
// public:
//     class Sentinel { };
//     class Iterator {
//         friend class RunningStatement;
//
//     public:
//         const Row& operator*()
//         {
//             return r;
//         }
//         bool operator==(const Sentinel&) const
//         {
//             return r.has_value();
//         }
//         Iterator& operator++()
//         {
//             r = s.stmt.next_row();
//             return *this;
//         }
//
//     private:
//         Iterator(RunningStatement& s)
//             : s(s)
//             , r(s.stmt)
//         {
//         }
//         RunningStatement& s;
//         Row r;
//     };
//     Iterator begin() { return { *this }; };
//     Sentinel end() { return {}; }
//
//     ~RunningStatement()
//     {
//         stmt.reset();
//     }
//
// private:
//     RunningStatement(SQLite::Statement& stmt);
//     Statement& stmt;
// };

template <typename T>
Column::operator T() const
{
    try {
        return static_cast<T>(ColumnConverter(*this));
    } catch (const std::exception& e) {
        spdlog::error(e.what());
        spdlog::error("Database error, cannot convert value");
        std::terminate();
    }
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
inline T Row::get(int index) const
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

template <typename T>
inline void Statement::bind(const int index, const T& t)
{
    struct Binder {
        using Stmt = SQLite::Statement;
        Binder(Stmt& stmt)
            : stmt(stmt)
        {
        }
        void bind_param(int i, const auto& a)
        {
            stmt.bind(i, a);
        }
        void bind_param(const int i, std::span<const uint8_t> s)
        {
            stmt.bind(i, s.data(), s.size());
        }
        void bind_param(const int i, const std::string& s)
        {
            stmt.bind(i, s.data(), s.size());
        }
        auto bind(int i, const auto& a)
        {
            bind_param(i, bind_convert::convert(a));
        }
        Stmt& stmt;
    };
    Binder(*this).bind(index, t);
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
    bind_multiple(std::forward<Types>(types)...);
    auto nchanged = exec();
    reset();
    assert(nchanged >= 0);
    return nchanged;
}

template <typename... Types, typename Lambda>
void Statement::for_each(Lambda lambda, Types&&... types)
{
    bind_multiple(std::forward<Types>(types)...);
    while (true) {
        auto r { next_row() };
        if (!r.has_value())
            break;
        lambda(r);
    }
    reset();
}


// template <typename... Types>
// auto Statement::loop(Types&&... types)
// {
//     recursive_bind<1>(std::forward<Types>(types)...);
//     return RunningStatement { *this };
// }
}
