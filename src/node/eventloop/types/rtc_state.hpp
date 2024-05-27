#pragma once
#include "transport/webrtc/sdp_util.hpp"
#include <optional>
struct RTCState {

    size_t total { 0 };
    std::optional<IdentityIps> ips;
    [[nodiscard]] std::optional<IP> get_ip(IpType t) const
    {
        using res_t = std::optional<IP>;
        if (!ips)
            return res_t {};
        switch (t) {
        case IpType::v4: {
            auto& v { ips->get_ip4() };
            if (v)
                return *v;
            break;
        }
        case IpType::v6: {
            auto& v { ips->get_ip6() };
            if (v)
                return *v;
            break;
        }
        }
        return {};
    };
};
