#pragma once

#include "general/tcp_util.hpp"
#include <limits>
#include <map>

class PerIpCounter {
public:
    bool insert(IPv4 ip, size_t max = std::numeric_limits<size_t>::max());
    void erase(IPv4);
    size_t count(IPv4);
    const auto& data() const { return counts; }

private:
    std::map<IPv4, size_t> counts;
};
