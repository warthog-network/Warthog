#include "ban_cache.hpp"

void BanCache::clear()
{
    map.clear();
    iters = {};
};

void BanCache::set(const IP& ip, uint32_t banUntil)
{
    auto p = map.emplace(ip, banUntil);
    if (p.second) { // newly inserted
        iters.push(p.first);
        if (iters.size() > maxSize) {
            map.erase(iters.front());
            iters.pop();
        }
    } else {
        auto& expires = p.first->second;
        if (expires < banUntil)
            expires = banUntil;
    }
}

auto BanCache::get(const IP& ip) -> std::optional<Match>
{
    auto iter = map.find(ip);
    if (iter == map.end())
        return {};
    return Match { iter->second };
}
