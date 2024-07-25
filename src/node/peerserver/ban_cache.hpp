#pragma once

#include "general/rate_limiter.hpp"
#include "general/timestamp.hpp"
#include "transport/helpers/ip.hpp"
#include <cstddef>
#include <map>
#include <queue>

namespace bancache {

using time_point = std::chrono::steady_clock::time_point;
template <typename key_t, typename extra_t>
class BasicCache {
protected:
    struct mapval_t;
    using map_t = std::map<key_t, mapval_t>;
    using multimap_t = std::multimap<time_point, typename map_t::iterator>;
    struct mapval_t {
        multimap_t::iterator iter;
        extra_t expires_generator;
    };
    map_t map;
    multimap_t byTimepoint;

protected:
    template <typename... T>
    [[nodiscard]] std::pair<extra_t&, bool> set_expiration(key_t ip, T&&... t)
    {
        auto [iter, inserted] { map.try_emplace(ip) };
        if (!inserted) {
            byTimepoint.erase(iter->second.iter);
        }
        time_point expires { iter->second.expires_generator(std::forward<T>(t)...) };
        iter->second.iter = byTimepoint.insert({ expires, iter });
        return { iter->second.expires_generator, inserted };
    }

public:
    void clear()
    {
        map.clear();
        byTimepoint.clear();
    }
    [[nodiscard]] std::optional<time_point> lookup_expiration(key_t k) const
    {
        auto iter { map.find(k) };
        if (iter != map.end())
            return iter->second.iter->first;
        return {};
    }
    [[nodiscard]] bool is_expired(key_t k, time_point tp = std::chrono::steady_clock::now()) const
    {
        auto e { lookup_expiration(k) };
        return e >= tp;
    }
    void prune(time_point ts, const auto& key_cb)
    {
        auto iter = byTimepoint.begin();
        for (; iter != byTimepoint.end() && iter->first <= ts; ++iter) {
            map.erase(iter->second);
            const auto& key { iter->second->first };
            key_cb(key);
        }
        byTimepoint.erase(byTimepoint.begin(), iter);
    }
    void prune(time_point ts = std::chrono::steady_clock::now())
    {
        prune(ts, [](auto&) {});
    }
};

namespace single_bancache {
    struct ExpiresGenerator {
        time_point operator()(time_point tp) { return tp; }
    };
    template <typename key_t>
    class SignleBanCache : public BasicCache<key_t, ExpiresGenerator> {
    public:
        auto set_expiration(key_t k, time_point tp)
        {
            return BasicCache<key_t, ExpiresGenerator>::set_expiration(k, tp);
        }
    };
}
namespace ratelimit_cache {
    struct ExpiresGenerator {
    public:
        time_point operator()()
        {
            r.count_event();
            return r.cooldown_tp();
        }
        bool is_limited() const { return r.limited(); }
        ExpiresGenerator()
            : r(std::chrono::minutes(5), 100) {};

    private:
        RateLimiter r;
    };
    template <typename key_t>
    class RatelimitCache : public BasicCache<key_t, ExpiresGenerator> {
    public:
        using base_t = BasicCache<key_t, ExpiresGenerator>;

        auto count_ban(key_t k)
        {
            return base_t::set_expiration(k);
        }
    };
}

using ratelimit_cache::RatelimitCache;
using single_bancache::SignleBanCache;

class BanCache {
public:
    void set(const IP& ip, time_point banUntil);
    void set(const IP& ip, Timestamp ts);
    using Match = time_point;
    std::optional<Match> get_expiration(const IP& ipl);
    void clear();

private:
    std::optional<Match> get_expiration_internal(const IPv4&);
    std::optional<Match> get_expiration_internal(const IPv6&);
    void set_internal(const IPv4&, time_point);
    void set_internal(const IPv6&, time_point);

private: // private data
    struct BanHandleData {
        std::optional<time_point> banUntil;
        size_t count { 0 };
    };
    SignleBanCache<IPv4> banmapv4;
    SignleBanCache<IPv6::BanHandle48> banmapv6_48;
    SignleBanCache<IPv6::BanHandle32> banmapv6_32;
    RatelimitCache<IPv6::BanHandle32> ratelimitCache;
};
}
using bancache::BanCache;
