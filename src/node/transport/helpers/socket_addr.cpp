#include "socket_addr.hpp"

std::string Sockaddr::to_string() const
{
    return std::visit([](auto& sockAddr) {
        return sockAddr.to_string();
    },
        data);
}

IP Sockaddr::ip() const
{
    return std::visit([](auto& sockAddr) -> IP {
        return sockAddr.ip;
    },
        data);
}

uint16_t Sockaddr::port() const
{
    return std::visit([](auto& sockAddr) {
        return sockAddr.port;
    },
        data);
}
