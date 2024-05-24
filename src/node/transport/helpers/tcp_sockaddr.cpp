#include "tcp_sockaddr.hpp"
#include "general/byte_order.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <stdexcept>

TCPSockaddrBase::TCPSockaddrBase(Reader& r)
    : ip(r)
    , port(r.uint16())
{
}

Writer& operator<<(Writer& w, const TCPSockaddrBase& addr){
    return w<<addr.ip<<addr.port;
}

std::string TCPSockaddr::to_string() const
{
    return "tcp://" + ip.to_string() + ":" + std::to_string(port);
}

std::string WSSockaddr::to_string() const
{
    return "ws://" + ip.to_string() + ":" + std::to_string(port);
}

#ifndef DISABLE_LIBUV
#include <uv.h>
sockaddr TCPSockaddrBase::sock_addr() const
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

TCPSockaddrBase::operator sockaddr() const { return sock_addr(); }
#endif
