#pragma once

#include "transport/helpers/ip.hpp"
#include <cstddef>
#include <map>
#include <queue>
class BanCache {
public:
    BanCache(size_t maxSize = 1000)
        : maxSize(maxSize) {};
    void set(const IP& ip, uint32_t banUntil);
    struct Match {
        uint32_t banUntil;
    };
    std::optional<Match> get(const IP& ipl);
    void clear();

private:
    std::map<IP, uint32_t> map;
    std::queue<decltype(map)::iterator> iters;
    const size_t maxSize;
};
