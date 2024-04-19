#include "socket_addr.hpp"

std::string Sockaddr::to_string() const
{
    return std::visit([](auto& sockAddr) {
        return sockAddr.to_string();
    },
        data);
}
IPv4 Sockaddr::ipv4() const{
    return std::visit([](auto& sockAddr) {
        return sockAddr.ipv4;
    },
        data);
}

uint16_t Sockaddr::port() const{
    return std::visit([](auto& sockAddr) {
        return sockAddr.port;
    },
        data);
}
