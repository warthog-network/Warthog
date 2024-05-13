#include "connect_request.hpp"
#include "transport/connect_request.hpp"

WebRTCConnectRequest make_request(const WSSockaddr& connectTo, steady_duration sleptFor)
{
    return { std::move(connectTo), sleptFor };
}

WebRTCConnectRequest::operator ConnectRequest() const
{
    return { address, sleptFor };
}
