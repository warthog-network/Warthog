#pragma once
#include "transport/helpers/ipv4.hpp"
#include "general/errors.hpp"
struct OffenseEntry {
    IPv4 ip;
    uint32_t timestamp;
    Error offense;
};
