#pragma once
#include "ipv4.hpp"
#include "ipv6.hpp"
#include <variant>

class IP {
public:
    enum class type { v4,
        v6 };
    using variant_t = std::variant<IPv4, IPv6>;

    auto visit(auto lambda) const
    {
        return std::visit(lambda, data);
    }
    std::string to_string() const
    {
        return visit([](auto ip) {
            return ip.to_string();
        });
    }
    bool is_localhost() const
    {
        return visit([](auto ip) {
            return ip.is_localhost();
        });
    }

    operator std::string() const { return to_string(); }
    auto operator<=>(const IP&) const = default;
    bool is_v6() const { return std::holds_alternative<IPv6>(data); }
    bool is_v4() const { return std::holds_alternative<IPv4>(data); }
    friend Writer& operator<<(Writer&, const IP&);
    size_t byte_size() const
    {
        return 1 + std::visit([](auto ip) { return ip.byte_size(); }, data);
    }
    IP(Reader& r);
    IP(IPv4 ip)
        : data(ip) {};
    IP(IPv6 ip)
        : data(ip) {};
    static std::optional<IP> parse(std::string_view s)
    {
        if (auto ipv4 { IPv4::parse(s) })
            return IP { *ipv4 };
        if (auto ipv6 { IPv6::parse(s) })
            return IP { *ipv6 };
        return {};
    }

    auto& vairiant() const { return data; }
    auto type() const
    {
        return visit([](auto& ip) { return ip.type(); });
    }

private:
    variant_t data;
};
