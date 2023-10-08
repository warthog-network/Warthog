#pragma once
#include "errors.hpp"
#include "general/byte_order.hpp"
#include "view.hpp"
#include <array>
#include <string>
#include <vector>

void serialize_hex(uint32_t number, char* out);
void serialize_hex(const uint8_t* data, size_t size, char* out);
std::string serialize_hex(const uint8_t* data, size_t size);

template <size_t N>
std::string serialize_hex(const std::array<uint8_t, N>& arr)
{
    return serialize_hex(arr.data(), arr.size());
}

template <size_t N>
std::string serialize_hex(View<N> v)
{
    const uint8_t* p = v.data();
    return serialize_hex(p, v.size());
}

[[nodiscard]] inline std::string serialize_hex(const std::vector<uint8_t>& vec)
{
    return serialize_hex(vec.data(), vec.size());
}
inline std::string serialize_hex(uint32_t v)
{
    uint32_t network = hton32(v);
    return serialize_hex((const uint8_t*)&network, 4);
}

bool parse_hex(std::string_view in, uint8_t* out, size_t out_size);

template <size_t N>
bool parse_hex(std::string_view in, std::array<uint8_t, N>& out)
{
    return parse_hex(in, out.data(), out.size());
}

template <size_t N>
std::array<uint8_t, N> hex_to_arr(std::string_view in)
{
    std::array<uint8_t, N> out;
    if (!parse_hex(in, out.data(), out.size()))
        throw Error(EMALFORMED);
    return out;
}

inline bool parse_hex(std::string_view in, std::vector<uint8_t>& out)
{
    out.resize(in.size() / 2);
    return parse_hex(in, out.data(), in.size() / 2);
}

inline std::vector<uint8_t> hex_to_vec(std::string_view in)
{
    std::vector<uint8_t> out;
    out.resize(in.size() / 2);
    if (!parse_hex(in, out.data(), in.size() / 2))
        throw Error(EMALFORMED);
    return out;
}
