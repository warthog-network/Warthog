#pragma once

#include "transport/helpers/socket_addr.hpp"
#include "helpers/connect_request_base.hpp"
struct ConnectRequest {
    const TCPSockaddr address;
    const steady_duration sleptFor;
};
