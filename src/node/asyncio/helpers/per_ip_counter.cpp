#include "per_ip_counter.hpp"
#include <cassert>

bool PerIpCounter::insert(IPv4 ip, size_t max)
{
    size_t& count = counts.try_emplace(ip, 0).first->second;
    if (count >= max)
        return false;
    count += 1;
    return true;
}
void PerIpCounter::erase(IPv4 ip)
{
    auto iter = counts.find(ip);
    if (iter == counts.end()) {
        return;
    }
    size_t& count = iter->second;
    assert(count != 0);
    count -= 1;
    if (count == 0) {
        counts.erase(iter);
    }
}

size_t PerIpCounter::count(IPv4 ip)
{
    auto iter = counts.find(ip);
    if (iter == counts.end())
        return 0;
    size_t& count = iter->second;
    assert(count != 0);
    return count;
}
