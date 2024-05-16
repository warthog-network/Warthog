#include "peer_rtc_state.hpp"

namespace rtc_state {

Identity::Identity(std::vector<IP> ips)
{
    for (auto& ip : ips)
        std::visit([&](auto ip) { unverified.insert(ip); }, ip.vairiant());
}
const IPv6* Identity::verified_v4(){

}
const IPv4* Identity::verified_v6(){

}
}
