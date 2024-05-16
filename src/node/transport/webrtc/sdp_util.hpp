#pragma once
#include "rtc/rtc.hpp"
#include "transport/helpers/ip.hpp"
#include <string>
#include <vector>

class SDPFilter;
class Reader;
class Writer;
class FilteredSDP {
public:
    operator const std::string&() const { return dsc; }
    const std::string& to_string() { return *this; }

private:
    friend class SDPFilter;
    FilteredSDP(std::string s)
        : dsc(std::move(s))
    {
    }
    std::string dsc;
};

class SDPFilter {
public:
    SDPFilter(std::string s)
        : dsc(std::move(s))
    {
    }
    FilteredSDP filter() const;
    std::vector<IP> udp_ips() const;

private:
    std::string dsc;
};

template <typename callback_t>
requires std::is_invocable_v<callback_t, std::vector<IP>>
void fetch_id(callback_t cb, bool stun = false)
{
    rtc::Configuration cfg;
    if (stun)
        cfg.iceServers.push_back({ "stun:stun.l.google.com:19302" });

    auto pc = std::make_shared<rtc::PeerConnection>(cfg);
    pc->onGatheringStateChange(
        [pc, on_result = std::move(cb)](rtc::PeerConnection::GatheringState state) mutable {
            if (state == rtc::PeerConnection::GatheringState::Complete) {
                SDPFilter sdp(pc->localDescription().value());
                on_result(sdp.udp_ips());
            }
        });
    auto dc { pc->createDataChannel("") };
}

struct IdentityIps {
    IdentityIps(Reader& r);
    IdentityIps() {};
    friend Writer& operator<<(Writer&, const IdentityIps&);
    void assign(IP);
    auto& get_ip6() const { return ipv6; };
    auto& get_ip4() const { return ipv4; };

private:
    void assign(IPv4 ip) { ipv4 = std::move(ip); }
    void assign(IPv6 ip) { ipv6 = std::move(ip); }
    size_t byte_size() const;
    std::optional<IPv4> ipv4;
    std::optional<IPv6> ipv6;
};
