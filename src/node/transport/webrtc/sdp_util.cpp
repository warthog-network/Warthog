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

}

FilteredSDP SDPFilter::filter() const
{
    std::string out;
    out.reserve(dsc.size());
    foreach_line(dsc, [&out](std::string_view line) {
        auto c { udp_candidate_ip(line) };
        if (!c.candidate || c.udp_ip) {
            out += line;
        }
    });
    return out;
}

std::vector<IP> SDPFilter::udp_ips() const
{
    std::vector<IP> out;
    foreach_line(dsc, [&out](std::string_view line) {
        auto c { udp_candidate_ip(line) };
        if (auto ip { c.udp_ip })
            out.push_back({ *ip });
    });
    return out;
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
