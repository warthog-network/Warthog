// #pragma once
// #include "transport/helpers/tcp_sockaddr.hpp"
// #include <vector>
//
// namespace address_manager {
//
// class FlatAddressSet {
//     size_t maxAddresses = 500;
//     std::vector<TCPSockaddr> vec;
//
// public:
//     const std::vector<TCPSockaddr>& data() const { return vec; }
//     size_t size() const { return vec.size(); }
//     void clear() { vec.clear(); }
//     bool full() { return vec.size() < maxAddresses; }
//     bool contains(TCPSockaddr a);
//     void insert(TCPSockaddr& a);
//     void erase(TCPSockaddr a);
// };
// }
