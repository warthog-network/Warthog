#include "ip.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"

Writer& operator<<(Writer& w, const IP& ip)
{
    w << uint8_t(ip.is_v4());
    std::visit([&w](auto ip) {
        w << ip;
    },
        ip.data);
    return w;
}

IP::IP(Reader& r)
    : data(r.uint8() != 0 ? variant_t(IPv4(r)) : variant_t(IPv6(r)))
{
}
