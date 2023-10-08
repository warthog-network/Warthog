#include "hex.hpp"

void serialize_hex(uint32_t number, char* out)
{
    uint32_t tmp = hton32(number);
    serialize_hex((const uint8_t*)&tmp, 4, out);
};

void serialize_hex(const uint8_t* data, size_t size, char* out)
{
    constexpr const char* h = "0123456789abcdef";
    for (size_t i = 0; i < size; ++i) {
        out[2 * i] = h[data[i] >> 4];
        out[2 * i + 1] = h[data[i] & 15];
    }
}

std::string serialize_hex(const uint8_t* data, size_t size)
{
    std::string out;
    out.resize(2 * size);
    serialize_hex(data, size, out.data());
    return out;
}

namespace {
inline uint8_t hexdigit(char c, bool& valid)
{
    switch (c) {
    case '0':
        return 0;
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    case '6':
        return 6;
    case '7':
        return 7;
    case '8':
        return 8;
    case '9':
        return 9;
    case 'a':
        return 10;
    case 'b':
        return 11;
    case 'c':
        return 12;
    case 'd':
        return 13;
    case 'e':
        return 14;
    case 'f':
        return 15;
    case 'A':
        return 10;
    case 'B':
        return 11;
    case 'C':
        return 12;
    case 'D':
        return 13;
    case 'E':
        return 14;
    case 'F':
        return 15;
    default:
        valid = false;
        return 0;
    }
}
}

bool parse_hex(std::string_view in, uint8_t* out, size_t out_size)
{
    if (in.size() != out_size * 2)
        return false;
    bool valid = true;
    for (size_t i = 0; i < out_size && valid; ++i) {
        out[i] = (hexdigit(in[2 * i], valid) << 4)
            + (hexdigit(in[2 * i + 1], valid));
    }
    return valid;
}
