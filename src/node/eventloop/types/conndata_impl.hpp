#pragma once
#include "asyncio/tcp/connection.hpp"
#include "conndata.hpp"

inline bool PeerState::erased()
{
    return this->c->eventloop_erased;
}
