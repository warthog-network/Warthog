#pragma once
#include "general/tcp_util.hpp"
#include "general/errors.hpp"
struct OffenseEntry {
    IPv4 ip;
    uint32_t timestamp;
    Error offense;
};
