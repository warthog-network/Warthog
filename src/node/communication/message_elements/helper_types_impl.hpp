#pragma once
#include "byte_size.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
#include "helper_types.hpp"
#include <cassert>

namespace messages {

template <typename T>
inline size_t vector_bytesize(const std::vector<T>& v)
{
    size_t len { 0 };
    for (auto& t : v) {
        len += byte_size(t);
    }
    return len;
}

template <typename T>
requires(std::is_standard_layout_v<T> && std::is_trivial_v<T>)
inline size_t vector_bytesize(const std::vector<T>& v)
{
    return sizeof(T) * v.size();
}

template <typename T>
requires(std::is_same_v<decltype(T::byte_size()), size_t>)
inline size_t vector_bytesize(const std::vector<T>& v)
{
    return T::byte_size() * v.size();
}


template <typename T>
inline size_t VectorRest<T>::byte_size() const
{
    return vector_bytesize<T>(*this);
}

template <typename T>
inline VectorRest<T>::VectorRest(Reader& r)
{
    while (r.remaining() > 0)
        std::vector<T>::push_back({ r });
}

template <typename T, typename len_t>
inline size_t VectorLentype<T, len_t>::byte_size() const
{
    return sizeof(len_t) + vector_bytesize<T>(*this);
}

template <typename T, typename len_t>
inline VectorLentype<T, len_t>::VectorLentype(Reader& r)
{
    size_t n { len_t { r } };
    this->reserve(n);
    for (size_t i = 0; i < n; ++i) {
        push_back({ r });
    }
}
template <typename T, typename len_t>
inline void VectorLentype<T, len_t>::push_back(T t)
{
    std::vector<T>::push_back(t);
    assert(this->size() <= maxlen);
}

template <typename T>
inline size_t Optional<T>::byte_size() const
{
    if (this->has_value())
        return 1 + ::byte_size(*this);
    return 1;
}

template <typename T>
inline Optional<T>::Optional(Reader& r)
{
    if (r.uint8()) {
        this->emplace(r);
    }
}

template <typename T>
inline ReadRest<T>::ReadRest(Reader& r)
    : T(r.rest())
{
}
}

template <typename T, typename len_t>
inline Writer& operator<<(Writer& w, const messages::VectorLentype<T, len_t>& vec)
{
    assert(vec.size() <= vec.maxlen);
    len_t len(vec.size());
    w << len;
    for (auto& e : vec) {
        w << e;
    }
    return w;
}

template <typename T>
inline Writer& operator<<(Writer& w, const messages::Optional<T>& o)
{
    if (o) {
        w << uint8_t(1) << *o;
    } else {
        w << uint8_t(0);
    }
    return w;
}

template <typename T>
inline Writer& operator<<(Writer& w, const messages::VectorRest<T>& vec)
{
    for (auto& e : vec)
        w << e;
    return w;
}
