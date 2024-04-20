#include "connect_request.hpp"
#include "transport/connect_request.hpp"

TCPConnectRequest make_request(const TCPSockaddr& connectTo, steady_duration sleptFor)
{
    return { std::move(connectTo), sleptFor };
}

TCPConnectRequest::operator ConnectRequest() const
{
    return { address, sleptFor };
}
