#include "tcp_util.hpp"
#include "general/byte_order.hpp"
#include "general/reader.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <stdexcept>

TCPSockaddr::TCPSockaddr(Reader& r)
    : ipv4(r)
    , port(r.uint16())
{
}

std::string TCPSockaddr::to_string() const
{
    return "tcp://" + ipv4.to_string() + ":" + std::to_string(port);
}

sockaddr TCPSockaddr::sock_addr() const
{
    sockaddr_in out;
    memset(&out, 0, sizeof(out));
    out.sin_family = AF_INET;
    out.sin_port = hton16(port);
#ifdef SIN6_LEN
    out.sin_len = sizeof(out);
#endif
    uint32_t ntmp = hton32(ipv4.data);

    static_assert(sizeof(struct in_addr) == 4);
    memcpy(&out.sin_addr.s_addr, &ntmp, 4);
    return *reinterpret_cast<sockaddr*>(&out);
}
