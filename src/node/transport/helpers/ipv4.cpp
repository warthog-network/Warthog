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

bool IPv4::is_valid() const{
    return data != INADDR_ANY && data != INADDR_NONE;
}
bool IPv4::is_loopback() const
{
    return at0() == 127;
}

bool IPv4::is_rfc1918() const
{
    // RFC1918
    return at0() == 10 || (at0() == 192 && at1() == 168) || (at0() == 172 && at1() >= 16 && at1() <= 31);
}
bool IPv4::is_rfc2544() const
{
    return at0() == 198 && (at1() == 18 || at1() == 19);
}
bool IPv4::is_rfc6598() const
{
    return at0() == 100 && at1() >= 64 && at1() <= 127;
}
bool IPv4::is_rfc5737() const
{
    return (at0() == 192 && at1() == 0 && at2() == 2) || (at0() == 198 && at1() == 51 && at2() == 100) || (at0() == 203 && at1() == 0 && at2() == 113);
}
bool IPv4::is_rfc3927() const
{
    return at0() == 169 && at1() == 254;
}

bool IPv4::is_routable() const
{
    return is_valid() && !(is_rfc1918() || is_rfc2544() || is_rfc3927() || is_rfc6598() || is_rfc5737() || is_loopback());
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
