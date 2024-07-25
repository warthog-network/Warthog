#include "ban_cache.hpp"

namespace bancache {
void BanCache::clear()
{
    banmapv4.clear();
    banmapv6_48.clear();
    banmapv6_32.clear();
    ratelimitCache.clear();
};

void BanCache::set_internal(const IPv4& ip, time_point banUntil)
{
    auto [extra, inserted] { banmapv4.set_expiration(ip, banUntil) };
    if (inserted) {
        banmapv4.prune();
    }
}

void BanCache::set_internal(const IPv6& ip, time_point banUntil)
{
    auto [extra, inserted] { banmapv6_48.set_expiration(ip.ban_handle48(), banUntil) };
    if (inserted)
        banmapv6_48.prune();
    auto& e { ratelimitCache.count_ban(ip.ban_handle32()).first };
    using namespace std::chrono;
    if (e.is_limited())
        banmapv6_32.set_expiration(ip.ban_handle32(), steady_clock::now() + minutes(20));
    banmapv6_32.prune();
    ratelimitCache.prune();
}

void BanCache::set(const IP& ip, time_point banUntil)
{
    ip.visit([&](auto& ip) { return set_internal(ip, banUntil); });
}

void BanCache::set(const IP& ip, Timestamp ts)
{
    set(ip, ts.steady_clock_time_point());
}

auto BanCache::get_expiration_internal(const IPv4& ip) -> std::optional<Match>
{
    return banmapv4.lookup_expiration(ip);
}

auto BanCache::get_expiration_internal(const IPv6& ip) -> std::optional<Match>
{
    return std::max(banmapv6_48.lookup_expiration(ip.ban_handle48()),
        banmapv6_32.lookup_expiration(ip.ban_handle32()));
}

auto BanCache::get_expiration(const IP& ip) -> std::optional<Match>
{
    return ip.visit([&](auto& ip) { return get_expiration_internal(ip); });
}
}
