#pragma once
#include "general/serializer_fwd.hxx"
#include "transport/helpers/ip.hpp"
#include <string>
#include <vector>

class Reader;
class Writer;

namespace sdp_filter {

[[nodiscard]] std::vector<IP> udp_ips(std::string_view);
[[nodiscard]] wrt::optional<IP> load_ip(std::string_view);
[[nodiscard]] wrt::optional<std::string> only_udp_ip(const IP&, std::string_view);

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
    IdentityIps() { };
    void serialize(Serializer auto& s) const
    {
        s << ipv4 << ipv6;
    };
    bool assign_if_routable(IP);
    [[nodiscard]] static IdentityIps from_sdp(const std::string& sdp);
    auto& get_ip4() const { return ipv4; };
    auto& get_ip6() const { return ipv6; };
    wrt::optional<IP> get_ip_with_type(IpType t) const
    {
        using enum IpType;
        if (t == v4) {
            if (auto ip { get_ip4() })
                return *ip;
        } else { // t==v6
            if (auto ip { get_ip6() })
                return *ip;
        }
        return {};
    }
    struct Pattern {
        bool ipv4;
        bool ipv6;
    };
    [[nodiscard]] Pattern pattern() const { return { ipv4.has_value(), ipv6.has_value() }; }
    [[nodiscard]] bool has_value() const { return ipv4.has_value() || ipv6.has_value(); }

    size_t byte_size() const;

private:
    void assign(IPv4 ip) { ipv4 = std::move(ip); }
    void assign(IPv6 ip) { ipv6 = std::move(ip); }
    wrt::optional<IPv4> ipv4;
    wrt::optional<IPv6> ipv6;
};
