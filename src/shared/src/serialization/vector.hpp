#pragma once
#include "general/serializer_fwd.hxx"
#include "serialization/byte_size.hpp"
#include <vector>

namespace serialization {

template <typename len_t>
concept is_vector_len_t = (std::is_same_v<len_t, uint8_t> || std::is_same_v<len_t, uint16_t> || std::is_same_v<len_t, uint32_t> || std::is_same_v<len_t, uint64_t>);

template <typename T, typename len_t>
requires(is_vector_len_t<len_t>)
struct VectorLentype : public std::vector<T> {
    static constexpr size_t max_len = std::numeric_limits<len_t>::max();
    VectorLentype() { }
    VectorLentype(std::vector<T> v)
        : std::vector<T>(std::move(v))
    {
    }
    VectorLentype(IsReader auto& r)
    {
        size_t n { len_t { r } };
        this->reserve(n);
        for (size_t i = 0; i < n; ++i)
            this->push_back({ r });
    }
    void serialize(Serializer auto&& s) const
    {
        auto n { this->size() };
        assert(n <= max_len);
        s << static_cast<len_t>(n);
        for (auto& e : *this)
            s << e;
    }
    size_t byte_size() const
    {
        size_t n { sizeof(len_t) };
        for (auto& e : *this)
            n += ::byte_size(e);
        return n;
    }
    void push_back(T t)
    {
        std::vector<T>::push_back(t);
        assert(this->size() <= max_len);
    }
};

template <typename T>
using Vector8 = VectorLentype<T, uint8_t>;

template <typename T>
using Vector16 = VectorLentype<T, uint16_t>;

template <typename T>
using Vector32 = VectorLentype<T, uint32_t>;
}
