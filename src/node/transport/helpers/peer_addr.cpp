#include "peer_addr.hpp"

std::string Peeraddr::to_string() const
{
    return visit([](auto& sockAddr) {
        return sockAddr.to_string();
    });
}
std::string_view Peeraddr::type_str() const{
    return visit([](auto& sockAddr) {
        return sockAddr.type_str();
    });
}

std::optional<IP> Peeraddr::ip() const
{
    return std::visit([](auto& sockAddr) -> std::optional<IP> {
        return sockAddr.ip;
    },
        data);
}

uint16_t Peeraddr::port() const
{
    return std::visit([](auto& sockAddr) {
        return sockAddr.port;
    },
        data);
}
