#include "tcp_sockaddr.hpp"
#include "general/byte_order.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <stdexcept>


Writer& operator<<(Writer& w, const Sockaddr4& addr){
    return w<<addr.ip<<addr.port;
}

std::string TCPPeeraddr::to_string() const
{
    return ip.to_string() + ":" + std::to_string(port);
}

std::string WSPeeraddr::to_string() const
{
    return ip.to_string() + ":" + std::to_string(port);
}

