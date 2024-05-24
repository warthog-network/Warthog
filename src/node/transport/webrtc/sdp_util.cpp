#include "sdp_util.hpp"
#include "communication/message_elements/byte_size.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
#include <optional>

namespace {
auto udp_candidate_ip(std::string_view s)
{
    struct Result {
        bool candidate { false };
        std::optional<std::string_view> udp_ip;
    };
    if (!s.starts_with("a=candidate:"))
        return Result {};
    size_t i = 11;
    const size_t N { s.size() };
    size_t spaces = 0;
    bool prevspace = false;
    for (i = 11; i < N; ++i) {
        auto c { s[i] };
        if (c == ' ') {
            if (prevspace == false) {
                spaces += 1;
                prevspace = true;
            }
        } else {
            if (prevspace == true) {
                if (spaces == 2) {
                    if (!s.substr(i).starts_with("UDP "))
                        return Result { true, {} };
                } else if (spaces == 4) {
                    auto sub { s.substr(i) };
                    if (auto n { sub.find(' ') }; n == std::string::npos) {
                        return Result { true, {} };
                    } else {
                        return Result { true, sub.substr(0, n) };
                    }
                }
            }
            prevspace = false;
        }
    }
    return Result { true, {} };
}

template <typename T>
requires std::is_invocable_r_v<void, T, std::string_view>
void foreach_line(std::string_view sdp, T&& callback)
{
    size_t i0 { 0 };
    for (size_t i = 0; i < sdp.size(); ++i) {
        auto c { sdp[i] };
        if (c == '\n') {
            auto line { sdp.substr(i0, i + 1 - i0) };
            callback(line);
            i0 = i + 1;
        }
    }
    if (i0 < sdp.size()) {
        auto line { sdp.substr(i0) };
        callback(line);
    }
}
template <typename callback_t>
requires std::is_invocable_v<callback_t, std::string_view>
[[nodiscard]] std::string filter_line(std::string_view sdp, callback_t good_line)
{
    std::string out;
    out.reserve(sdp.size());
    foreach_line(sdp, [&](std::string_view line) {
        if (good_line(line))
            out += line;
    });
    return out;
}

}

namespace sdp_filter {

// std::string filter_line(std::string_view sdp)
// {
//     std::string out;
//     out.reserve(sdp.size());
//     foreach_line(sdp, [&out](std::string_view line) {
//         auto c { udp_candidate_ip(line) };
//         if (!c.candidate || c.udp_ip) {
//             out += line;
//         }
//     });
//     return out;
// }

std::vector<IP> udp_ips(std::string_view sdp)
{
    std::vector<IP> out;
    foreach_line(sdp, [&out](std::string_view line) {
        auto c { udp_candidate_ip(line) };
        if (c.udp_ip) {
            if (auto ip { IP::parse(*c.udp_ip) })
                out.push_back({ *ip });
        }
    });
    return out;
}

std::optional<IP> load_ip(std::string_view sdp)
{
    auto ips { udp_ips(sdp) };
    if (ips.size() != 1)
        return {};
    return ips.front();
}

std::optional<std::string> only_udp_ip(const IP& ip, std::string_view sdp)
{
    std::vector<IP> out;
    return filter_line(sdp, [&ip](std::string_view line) -> bool {
        auto c { udp_candidate_ip(line) };
        if (!c.candidate)
            return true;
        if (!c.udp_ip)
            return false;
        return IP::parse(*c.udp_ip) == ip;
    });
}
}

OneIpSdp::OneIpSdp(std::string s)
    : sdpString(std::move(s))
    , _ip([&]() {
        auto ip { sdp_filter::load_ip(sdpString) };
        if (!ip.has_value())
            throw Error(ERTCUNIQUEIP);
        return *ip;
    }())
{
}

size_t IdentityIps::byte_size() const
{
    return ::byte_size(ipv4) + ::byte_size(ipv6);
}

IdentityIps::IdentityIps(Reader& r)
    : ipv4(r)
    , ipv6(r)
{
}

void IdentityIps::assign(IP ip)
{
    ip.visit([&](auto ip) { assign(ip); });
};

Writer& operator<<(Writer& w, const IdentityIps& id)
{
    return w << id.ipv4 << id.ipv6;
};
