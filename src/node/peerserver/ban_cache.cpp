#include "ban_cache.hpp"

void BanCache::clear()
{
    map.clear();
    iters = {};
};
void BanCache::set(IPv4 ip, const Value& v)
{
    auto [it, inserted] = map.try_emplace(ip.data, v);
    if (inserted) { // newly inserted
        iters.push(it);
        if (iters.size() > maxSize) {
            map.erase(iters.front());
            iters.pop();
        }
    } else {
        auto& ban = it->second;
        if (ban.timepoint < v.timepoint)
            ban = v;
    }
}

auto BanCache::get(IPv4 ip) const -> std::optional<Value>
{
    auto iter = map.find(ip.data);
    if (iter == map.end())
        return std::nullopt;
    return iter->second;
};
