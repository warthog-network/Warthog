#include "socket_addr.hpp"

std::string Sockaddr::to_string() const
{
    return visit([](auto& sockAddr) {
        return sockAddr.to_string();
    });
}
std::string_view Sockaddr::type_str() const{
    return visit([](auto& sockAddr) {
        return sockAddr.type_str();
    });
}

std::optional<IP> Sockaddr::ip() const
{
    return std::visit([](auto& sockAddr) -> std::optional<IP> {
        return sockAddr.ip();
    },
        data);
}

uint16_t Sockaddr::port() const
{
    return std::visit([](auto& sockAddr) {
        return sockAddr.port();
    },
        data);
}
