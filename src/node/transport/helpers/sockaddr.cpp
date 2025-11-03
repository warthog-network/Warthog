#include "sockaddr.hpp"
#include "general/reader.hpp"
#ifndef DISABLE_LIBUV
#include <uv.h>

wrt::optional<Sockaddr> Sockaddr::from_sockaddr_storage(const sockaddr_storage& sa){
    switch (sa.ss_family) {
    case AF_INET: {
        sockaddr_in* addr_i4 = (struct sockaddr_in*)&sa;
        IPv4 ip(ntoh32(uint32_t(addr_i4->sin_addr.s_addr)));
        uint16_t port{addr_i4->sin_port};
        return Sockaddr(ip,port);
    } break;
    case AF_INET6: {
        auto* addr_i6 = (struct sockaddr_in6*)&sa;
        auto ip{IPv6::from_data(addr_i6->sin6_addr.s6_addr)};
        uint16_t port{addr_i6->sin6_port};
        return Sockaddr(ip,port);
    }
    default:
        return {};
    }

}

#endif
Sockaddr4::Sockaddr4(Reader& r)
    : ip(r)
    , port(r.uint16())
{
}

#ifndef DISABLE_LIBUV
sockaddr Sockaddr4::sock_addr() const
{
    sockaddr_in out;
    memset(&out, 0, sizeof(out));
    out.sin_family = AF_INET;
    out.sin_port = hton16(port);
#ifdef SIN6_LEN
    out.sin_len = sizeof(out);
#endif
    uint32_t ntmp = hton32(ip.data);

    static_assert(sizeof(struct in_addr) == 4);
    memcpy(&out.sin_addr.s_addr, &ntmp, 4);
    return *reinterpret_cast<sockaddr*>(&out);
}

Sockaddr4::operator sockaddr() const { return sock_addr(); }
#endif
