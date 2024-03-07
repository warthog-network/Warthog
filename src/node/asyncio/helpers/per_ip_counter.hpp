#pragma once

#include "general/net/ipv4.hpp"
#include <limits>
#include <map>

class PerIpCounter {
public:
    bool insert(IPv4 ip, size_t max = std::numeric_limits<size_t>::max());
    void erase(IPv4);
    size_t count(IPv4) const;
    bool contains(IPv4 ip) const { return count(ip) > 0; }
    const auto& data() const { return counts; }

private:
    std::map<IPv4, size_t> counts;
};
