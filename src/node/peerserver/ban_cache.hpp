#pragma once

#include "transport/helpers/ipv4.hpp"
#include <cstddef>
#include <map>
#include <queue>
class BanCache {
public:
    BanCache(size_t maxSize = 1000)
        : maxSize(maxSize) {};
    void set(IPv4 ip, uint32_t banUntil);
    bool get(IPv4 ip, uint32_t& banUntil);
    void clear();

private:
    std::map<uint32_t, uint32_t> map;
    std::queue<decltype(map)::iterator> iters;
    const size_t maxSize;
};
