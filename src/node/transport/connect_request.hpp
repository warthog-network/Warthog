#pragma once

#include "transport/helpers/socket_addr.hpp"
#include "helpers/connect_request_base.hpp"
struct ConnectRequest {
    const Sockaddr address;
    const steady_duration sleptFor;
};
