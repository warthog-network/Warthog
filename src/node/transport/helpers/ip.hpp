#pragma once
#include "ipv4.hpp"
#include "ipv6.hpp"
#include <variant>

class IP {
public:
    class BanHandle {
    public:
        BanHandle(IPv4 ip)
            : variant(ip)
        {
        }
        BanHandle(IPv6::BanHandle32 v)
            : variant(v)
        {
        }
        BanHandle(IPv6::BanHandle48 v)
            : variant(v)
        {
        }
        auto visit(auto lambda) const
        {
            return std::visit(lambda, variant);
        }
        std::string to_string() const
        {
            return visit([](auto& handle) { return handle.to_string(); });
        }

    private:
        using variant_t = std::variant<IPv4, IPv6::BanHandle32, IPv6::BanHandle48>;
        variant_t variant;
    };
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
    bool is_loopback() const
    {
        return visit([](auto ip) {
            return ip.is_loopback();
        });
    }
    bool is_routable() const
    {
        return visit([](auto ip) { return ip.is_routable(); });
    }

    operator std::string() const { return to_string(); }
    auto operator<=>(const IP&) const = default;
    bool is_v6() const { return std::holds_alternative<IPv6>(data); }
    bool is_v4() const { return std::holds_alternative<IPv4>(data); }
    const IPv4& get_v4() const
    {
        return std::get<IPv4>(data);
    }
    const IPv6& get_v6() const
    {
        return std::get<IPv6>(data);
    }
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
    [[nodiscard]] static wrt::optional<IP> parse(std::string_view s)
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
