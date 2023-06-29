#pragma once
#include "general/tcp_util.hpp"
#include <vector>

namespace address_manager {

class FlatAddressSet {
    size_t maxAddresses = 500;
    std::vector<EndpointAddress> vec;

public:
    const std::vector<EndpointAddress>& data() const { return vec; }
    size_t size() const { return vec.size(); }
    void clear() { vec.clear(); }
    bool full() { return vec.size() < maxAddresses; }
    bool contains(EndpointAddress a);
    void insert(EndpointAddress& a);
    void erase(EndpointAddress a);
};
}
