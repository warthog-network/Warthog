#pragma once
#include "SQLiteCpp/SQLiteCpp.h"
#include <optional>
#include <vector>
class Hash;
class Height;
class HistoryId;
class Address;
class BodyContainer;
class Header;
class NonzeroHeight;
class AccountId;
struct IsUint64;
class BlockId;
class Funds;
class Worksum;
class Timestamp;
template <size_t N>
struct View;

namespace sqlite {
struct Column : public SQLite::Column {
    operator Hash() const;
    operator Height() const;
    operator HistoryId() const;
    template <size_t size>
    std::array<uint8_t, size> get_array() const;
    std::vector<uint8_t> get_vector() const;
    operator std::vector<uint8_t>() const;
    operator Address() const;
    operator BodyContainer() const;
    operator Header() const;
    operator NonzeroHeight() const;
    operator int64_t() const;
    operator AccountId() const;
    operator IsUint64() const;
    operator BlockId() const;
    operator Funds() const;
    operator uint64_t() const;
};

class Statement;
class RunningStatement;
class Row {

private: // data
    std::reference_wrapper<Statement> st;
    bool hasValue;

public:
    friend class Statement;
    friend class RunningStatement;
    Column operator[](int index) const;
    template <typename T>
    T get(int index);
    template <size_t N>
    std::array<uint8_t, N> get_array(int index) const;
    std::vector<uint8_t> get_vector(int index) const;
    template <typename T>
    operator std::optional<T>();
    bool has_value() const { return hasValue; }
    auto process(auto lambda) const;

private:
    void value_assert() const;
    Row(Statement& st);
    Statement& statement() const;
};

class Statement : public SQLite::Statement {
public:
    using SQLite::Statement::Statement;
    Column getColumn(const int aIndex);
    void bind(const int index, const Worksum& ws);
    void bind(const int index, const std::vector<uint8_t>& v);
    template <size_t N>
    void bind(const int index, std::array<uint8_t, N> a);
    template <size_t N>
    void bind(const int index, View<N> v);
    void bind(const int index, Funds f);
    void bind(const int index, int64_t id);
    void bind(const int index, IsUint64 id);
    void bind(const int index, BlockId id);
    void bind(const int index, Height id);
    void bind(const int index, const std::string&);
    void bind(const int index, Timestamp);
    template <size_t i>
    void recursive_bind();
    template <size_t i, typename T, typename... Types>
    void recursive_bind(T&& t, Types&&... types);
    template <typename... Types>
    uint32_t run(Types&&... types);

    // private:
    struct SingleResult : public Row {
        using Row::Row;
        ~SingleResult()
        {
            if (hasValue)
                assert(statement().executeStep() == false);
            statement().reset();
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
    void for_each(Lambda lambda, Types&&... types);

    template <typename... Types>
    auto loop(Types&&... types);
};

}
