#pragma once
#include "rtc/rtc.hpp"
#include "transport/helpers/ip.hpp"
#include <string>
#include <vector>

class Reader;
class Writer;

namespace sdp_filter {

[[nodiscard]] std::vector<IP> udp_ips(std::string_view);
[[nodiscard]] std::optional<IP> load_ip(std::string_view);
[[nodiscard]] std::optional<std::string> only_udp_ip(const IP&, std::string_view);

}

class OneIpSdp {
public:
    OneIpSdp(std::string s);
    [[nodiscard]] auto sdp() const { return sdpString; }
    [[nodiscard]] auto ip() const { return _ip; }

private:
    static IP gen_ip();
    std::string sdpString;
    IP _ip;
};


struct IdentityIps {
    IdentityIps(Reader& r);
    IdentityIps() {};
    friend Writer& operator<<(Writer&, const IdentityIps&);
    bool assign_if_routable(IP);
    auto& get_ip6() const { return ipv6; };
    auto& get_ip4() const { return ipv4; };

    size_t byte_size() const;

private:
    void assign(IPv4 ip) { ipv4 = std::move(ip); }
    void assign(IPv6 ip) { ipv6 = std::move(ip); }
    std::optional<IPv4> ipv4;
    std::optional<IPv6> ipv6;
};
