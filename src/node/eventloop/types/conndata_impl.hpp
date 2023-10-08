#pragma once
#include "asyncio/connection.hpp"
#include "conndata.hpp"

inline bool PeerState::erased()
{
    return this->c->eventloop_erased;
}
