#pragma once

#include "transport/helpers/ip.hpp"
#include <limits>
#include <map>

class PerIpCounter {
public:
    bool insert(IP ip, size_t max = std::numeric_limits<size_t>::max());
    void erase(IP);
    size_t count(const IP&) const;
    bool contains(IP ip) const { return count(ip) > 0; }
    const auto& data() const { return counts; }

private:
    std::map<IP, size_t> counts;
};
