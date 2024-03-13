#pragma once

#include "general/tcp_util.hpp"

class Pinned
{
public:
    Pinned ();
private:
    EndpointAddress endpointAddress;
};
