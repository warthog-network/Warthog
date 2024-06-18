#pragma once
#include "transport/helpers/ip.hpp"
#include "general/errors.hpp"
struct OffenseEntry {
    IP ip;
    uint32_t timestamp;
    Error offense;
};
