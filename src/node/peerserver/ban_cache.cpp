#include "ban_cache.hpp"

void BanCache::clear()
{
    map.clear();
    iters = {};
};
void BanCache::set(IPv4 ip, uint32_t banUntil)
{
    auto p = map.insert(std::make_pair(ip.data, banUntil));
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

bool BanCache::get(IPv4 ip, uint32_t& banUntil)
{
    auto iter = map.find(ip.data);
    if (iter == map.end())
        return false;
    banUntil = iter->second;
    return true;
};
