#include "connect_request.hpp"
#include "transport/connect_request.hpp"

WSConnectRequest make_request(const WSSockaddr& connectTo, steady_duration sleptFor)
{
    return { std::move(connectTo), sleptFor };
}

WSConnectRequest::operator ConnectRequest() const
{
    return { address, sleptFor };
}
