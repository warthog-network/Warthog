#include "connect_request.hpp"

WebRTCConnectRequest make_request(const WSSockaddr& connectTo, steady_duration sleptFor)
{
    return { std::move(connectTo), sleptFor };
}

