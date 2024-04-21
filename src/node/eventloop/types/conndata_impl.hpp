#pragma once
#include "conndata.hpp"

inline bool PeerState::erased()
{
    return this->c->eventloop_erased;
}
