#pragma once
#include "ipv4.hpp"
#include "ipv6.hpp"
#include <variant>

class IP {
public:
    using variant_t = std::variant<IPv4, IPv6>;
    std::string to_string() const
    {
        return std::visit([](auto ip) {
            return ip.to_string();
        },
            data);
    }
    operator std::string() const { return to_string(); }
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
    IP(std::string_view s)
        : data(parse(s)) {};

    auto& vairiant() const{return data;}
    auto visit(auto lambda) const {
        return std::visit(lambda,data);
    }
private:
    static variant_t parse(std::string_view s)
    {
        if (auto p { IPv4::parse(s) })
            return *p;
        return IPv6(s);
    }
    variant_t data;
};
