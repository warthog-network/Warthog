#pragma once

#include "general/error_time.hpp"
#include "general/errors.hpp"
#include "general/tcp_util.hpp"
#include <cstddef>
#include <map>
#include <queue>
class BanCache {
    using Value = ErrorTimepoint;

    using sc = std::chrono::steady_clock;

public:
    BanCache(size_t maxSize = 5000)
        : maxSize(maxSize) {};
    void set(IPv4 ip, const Value&);
    std::optional<Value> get(IPv4 ip) const;
    void clear();

private:
    using map_t = std::map<uint32_t, Value>;
    map_t map;
    std::queue<map_t::iterator> iters;
    const size_t maxSize;
};
