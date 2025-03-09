#pragma once
#include "conndata.hpp"

inline bool ConState::erased()
{
    return this->c->eventloop_erased;
}
