#include "peer_addr.hpp"

std::string Peerhost::to_string() const
{
    return std::visit([](auto& host) -> std::string {
        return host;
    },
        data);
}
std::string Peeraddr::to_string() const
{
    return visit([](auto& sockAddr) {
        return sockAddr.to_string_with_protocol();
    });
}
std::string_view Peeraddr::type_str() const
{
    return visit([](auto& sockAddr) {
        return sockAddr.type_str();
    });
}

wrt::optional<IP> Peeraddr::ip() const
{
    return std::visit([](auto& sockAddr) -> wrt::optional<IP> {
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

Peerhost Peeraddr::host() const
{
    return { visit([](auto& addr) -> Peerhost { return Peerhost{ addr.host() }; }) };
}
