#include "ipv6.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"

IPv6::IPv6(Reader& r)
    : data(r.view<16>())
{
}

Writer& operator<<(Writer& w, const IPv6& ip)
{
    return w << Range(ip.data);
}

std::string IPv6::to_string() const
{
    std::string out;
    out.resize(8 * 4 + 7);
    for (size_t i = 0; i < 8; ++i) {
        constexpr char map[] = "0123456789ABCDEF";
        size_t offset = 5 * i;
        uint8_t c = data[2 * i];
        out[offset] = map[c >> 4];
        out[offset + 1] = map[c & 15];
        c = data[2 * i + 1];
        out[offset + 2] = map[c >> 4];
        out[offset + 3] = map[c & 15];
        if (i < 7)
            out[offset + 4] = ':';
    }
    return out;
}
