#pragma once
#include "transport/tcp/tcp_sockaddr.hpp"
#include "general/errors.hpp"
struct OffenseEntry {
    IPv4 ip;
    uint32_t timestamp;
    Error offense;
};
