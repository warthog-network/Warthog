#pragma once
#include "general/reader.hpp"
#include "general/writer.hpp"
#include "helper_types.hpp"
#include "serialization/byte_size.hpp"
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

template <typename T>
inline size_t Optional<T>::byte_size() const
{
    if (this->has_value())
        return 1 + ::byte_size(**this);
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
