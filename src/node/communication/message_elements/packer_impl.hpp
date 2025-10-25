#pragma once
#include "communication/buffers/sndbuffer.hpp"
#include "packer.hpp"
#include "serialization/byte_size.hpp"
#include <cstddef>

template <size_t code, typename... Ts>
template <size_t... Is>
template <size_t i>
size_t MsgPacker<code, Ts...>::Internal<std::index_sequence<Is...>>::sum_byte_size(size_t cumsum) const
{
    if constexpr (i == sizeof...(Is))
        return cumsum;
    else
        return sum_byte_size<i + 1>(cumsum + ::byte_size(get<i>()));
}

template <size_t code, typename... Ts>
template <size_t... Is>
MsgPacker<code, Ts...>::Internal<std::index_sequence<Is...>>::operator Sndbuffer() const
{
    auto mw { MsgCode<code>::gen_msg(this->byte_size()) };
    return write_message<0>(mw);
}
