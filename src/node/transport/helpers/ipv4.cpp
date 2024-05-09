#include "ipv4.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
#ifndef DISABLE_LIBUV
#include "uv.h"
#endif

IPv4::IPv4(Reader& r)
    : data(r.uint32())
{
}

Writer& operator<<(Writer& w, const IPv4& ip)
{
    return w << ip.data;
}

#ifndef DISABLE_LIBUV
IPv4::IPv4(const sockaddr_in& sin)
    : IPv4(ntoh32(uint32_t(sin.sin_addr.s_addr)))
{
}
#endif

bool IPv4::is_valid(bool allowLocalhost) const
{
    uint32_t ndata = hton32(data);
    uint8_t* pdata = reinterpret_cast<uint8_t*>(&ndata);
    return data != 0 // ignore 0.0.0.0
        && (allowLocalhost || pdata[0] != 127)
        && pdata[0] != 192 // ignore 192.xxx.xxx.xxx
        && !(pdata[0] == 169 && pdata[1] == 254); // ignore 192.xxx.xxx.xxx
}

bool IPv4::is_localhost() const
{
    return data == (127 << 24) + 1;
}
// modified version of libuv's
// static int inet_pton4(const char *src, unsigned char *dst) {

std::string IPv4::to_string() const
{
    static const char fmt[] = "%u.%u.%u.%u";
    uint32_t ndata = hton32(data);
    uint8_t* src = reinterpret_cast<uint8_t*>(&ndata);
    char tmp[16];
    assert(snprintf(tmp, sizeof(tmp), fmt, src[0], src[1], src[2], src[3]) > 0);
    return { tmp };
}
