#include "ban_cache.hpp"

namespace bancache {
void BanCache::clear()
{
    banmapv4.clear();
    banmapv6_48.clear();
    banmapv6_32.clear();
    ratelimitCache.clear();
};

void BanCache::ban_internal(const IPv4& ip, ErrorTimepoint data)
{
    auto [extra, inserted] { banmapv4.ban(ip, data) };
    if (inserted) {
        banmapv4.prune();
    }
}

void BanCache::ban_internal(const IPv6& ip, ErrorTimepoint data)
{
    auto [extra, inserted] { banmapv6_48.ban(ip.ban_handle48(), data) };
    if (inserted)
        banmapv6_48.prune();
    auto& e { ratelimitCache.count(ip.ban_handle32()).first };
    using namespace std::chrono;
    if (e.is_limited()) {
        data.time_point() = steady_clock::now() + minutes(20);
        banmapv6_32.ban(ip.ban_handle32(), data);
    }
    banmapv6_32.prune();
    ratelimitCache.prune();
}

void BanCache::ban(const IP& ip, ErrorTimepoint banUntil)
{
    ip.visit([&](auto& ip) { return ban_internal(ip, banUntil); });
}

auto BanCache::get_expiration_internal(const IPv4& ip) -> std::optional<Timepoint>
{
    if (auto f { banmapv4.find(ip) })
        return f->timepoint;
    return {};
}

auto BanCache::get_expiration_internal(const IPv6& ip) -> std::optional<Timepoint>
{
    return std::max(banmapv6_48.lookup_expiration(ip.ban_handle48()), banmapv6_32.lookup_expiration(ip.ban_handle32()));
}

auto BanCache::get_expiration(const IP& ip) -> std::optional<Timepoint>
{
    return ip.visit([&](auto& ip) { return get_expiration_internal(ip); });
}
}
