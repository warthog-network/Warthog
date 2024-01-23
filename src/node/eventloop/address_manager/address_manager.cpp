#include "address_manager.hpp"
#include "asyncio/connection.hpp"
#include "global/globals.hpp"
#include <algorithm>
#include <future>
#include <random>

namespace address_manager {
using namespace std::chrono;
using namespace std::chrono_literals;
constexpr auto failedSleep = 60min;
constexpr auto successSleep = 60min;

Conref AddressManager::find(uint64_t id)
{
    auto iter = conndatamap.find(id);
    if (iter == conndatamap.end())
        return {};
    return ConrefIter { iter };
}

std::pair<int32_t, Conref> AddressManager::insert(Connection* c, HeaderDownload::Downloader& h, BlockDownload::Downloader& b, Timer& t)
{
    EndpointAddress a { c->peer_endpoint() };
    if (auto iter = byEndpoint.find(a); iter != byEndpoint.end()) {
        if (!c->inbound) {
            failedAddresses.erase(a);
            iter->second->second.verifiedEndpoint = true;
            just_verified(a, true);
        }
        return { EDUPLICATECONNECTION, {} };
    }

    // insert int conndatamap
    auto p = conndatamap.try_emplace(c->id,
        c, h, b, t);
    assert(p.second);
    Conref cr { p.first };
    c->dataiter = cr.iterator();

    if (cr->c->inbound)
        queue_verification(cr->c->peer_endpoint());
    else {
        just_verified(a, false);

        // set "connected" flag in verified list
        if (auto iter = verified.find(a); iter != verified.end()) {
            iter->second.outboundConnection = true;
        }
    }

    // adjust pinned entry
    auto iter = pinned.find(a);
    if (iter != pinned.end()) {
        remove_timer(iter);
        iter->second.sleepSeconds = 0;
    }
    assert(byEndpoint.try_emplace(a, p.first).second);

    return { 0, cr };
}
bool AddressManager::erase(Conndatamap::iterator iter)
{
    bool outbound = !iter->second.c->inbound;
    if (outbound || iter->second.verifiedEndpoint) {
        EndpointAddress a { iter->second.c->peer_endpoint() };
        peerServer.async_seen_peer(a);
    }
    EndpointAddress a { iter->second.c->peer_endpoint() };
    byEndpoint.erase(a);
    delayedDelete.push_back(iter);

    // set "connected" flag in verified list
    if (auto iter = verified.find(a); iter != verified.end()) {
        iter->second.outboundConnection = false;
        if (outbound) {
            iter->second.lastVerified = sc::now();
            set_timer(sc::now() + successSleep, iter);
        }
    }

    if (auto iter = pinned.find(a); iter != pinned.end()) {
        auto& pinState = iter->second;
        auto expires = std::max(sc::now() + seconds(pinState.sleepSeconds), iter->second.ratelimit_sleep());
        auto timer_iter = timer.emplace(expires, iter);
        assert(pinState.timer_iter == timer.end());
        pinState.timer_iter = timer_iter;
        return true;
    }
    return false;
}

void AddressManager::garbage_collect()
{
    for (auto iter : delayedDelete) {
        conndatamap.erase(iter);
    }
    delayedDelete.clear();
}

void AddressManager::remove_timer(decltype(pinned)::iterator iter)
{
    auto& timer_iter = iter->second.timer_iter;
    if (timer_iter == timer.end())
        return;
    timer.erase(timer_iter);
    timer_iter = timer.end();
}

std::optional<std::chrono::steady_clock::time_point> AddressManager::wakeup_time()
{
    if (timer.empty())
        return {};
    return timer.begin()->first;
}

std::vector<EndpointAddress> AddressManager::sample_verified(size_t N)
{
    // update cache if necessary
    if (!cacheExpire || *cacheExpire <= sc::now()) {
        cacheExpire = sc::now() + 1min;
        verifiedCache.clear();
        for (auto& [a, _] : verified) {
            verifiedCache.push_back(a);
        }
    }

    // sample from cache
    std::vector<EndpointAddress> out;
    std::sample(verifiedCache.begin(), verifiedCache.end(), std::back_inserter(out),
        N, std::mt19937 { std::random_device {}() });
    return out;
}

bool AddressManager::pin(EndpointAddress a)
{
    if (config().node.isolated)
        return true;
    auto p = pinned.try_emplace(a);
    if (!p.second)
        return false;
    auto iter = timer.emplace(sc::now(), p.first);
    p.first->second.timer_iter = iter;
    return true;
}

bool AddressManager::unpin(EndpointAddress a)
{
    auto iter = pinned.find(a);
    if (iter == pinned.end())
        return false;
    auto& t = iter->second.timer_iter;
    if (t != timer.end()) {
        timer.erase(t);
    }
    pinned.erase(iter);
    return true;
}

void AddressManager::pin(const std::vector<EndpointAddress>& as)
{
    for (auto a : as) {
        pin(a);
    }
}

namespace {
    auto interface_ips_v4()
    {
        std::vector<IPv4> out;
        uv_interface_address_t* info;
        int count;
        uv_interface_addresses(&info, &count);
        for (int i = 0; i < count; ++i) {
            uv_interface_address_t interface_a = info[i];
            if (interface_a.address.address4.sin_family == AF_INET) {
                out.push_back(interface_a.address.address4);
            }
        }
        uv_free_interface_addresses(info, count);
        return out;
    }
}

AddressManager::AddressManager(PeerServer& peerServer, const std::vector<EndpointAddress>& as)
    : peerServer(peerServer)
    , ownIps(interface_ips_v4())
{
    // get recently seen peers from db
    std::promise<std::vector<std::pair<EndpointAddress, uint32_t>>> p;
    auto future { p.get_future() };
    auto cb = [&p](std::vector<std::pair<EndpointAddress, uint32_t>>&& v) {
        p.set_value(std::move(v));
    };
    peerServer.async_get_recent_peers(std::move(cb), maxRecent);
    auto db_peers = future.get();
    int64_t nowts = now_timestamp();
    for (const auto& [a, timestamp] : db_peers) {
        auto p = verified.try_emplace(a, timer.end());
        assert(p.second);
        set_timer(sc::now(), p.first);
        auto& node = p.first->second;
        node.lastVerified = sc::now() - seconds((nowts - int64_t(timestamp)));
    }

    // pin
    pin(as);
}

bool AddressManager::on_failed_outbound(EndpointAddress a)
{
    pendingOutgoing.erase(a);
    if (byEndpoint.contains(a))
        return false;

    // failed addresses bookkeeping
    failedAddresses.insert(a);
    if (failedAddresses.size() > 1000) {
        failedAddresses.clear();
    }

    // verified addresses bookkeeping
    if (auto iter = verified.find(a); iter != verified.end()) {
        set_timer(sc::now() + failedSleep, iter);
    }

    // pinned bookkeeping
    if (auto iter = pinned.find(a); iter != pinned.end()) {
        auto& cs = iter->second;
        auto timer_iter = timer.emplace(sc::now() + seconds(cs.sleepSeconds), iter);
        remove_timer(iter);
        cs.timer_iter = timer_iter;
        cs.sleepSeconds = std::max(cs.sleepSeconds, std::min(2 * (cs.sleepSeconds + 1), size_t(5 * 60ul)));
        return true;
    }
    return false;
}

std::vector<EndpointAddress> AddressManager::pop_connect()
{
    auto now = sc::now();
    std::vector<EndpointAddress> out;

    while (pendingOutgoing.size() < maxPending) {
        if (timer.size() > 0 && timer.begin()->first < now) {
            auto& v = timer.begin()->second;
            if (std::holds_alternative<VerIter>(v)) {
                auto iter = std::get<VerIter>(v);
                remove_timer(iter);
                const EndpointAddress& a = iter->first;
                if (pendingOutgoing.contains(a))
                    continue;
                out.push_back(a);
                pendingOutgoing.try_emplace(a, now);
            } else if (std::holds_alternative<PinIter>(v)) {
                auto iter = std::get<PinIter>(v);
                remove_timer(iter);
                const EndpointAddress& a = iter->first;
                if (pendingOutgoing.contains(a))
                    continue;
                out.push_back(a);
                pendingOutgoing.try_emplace(a, now);
            }
        } else if (unverifiedAddresses.size() > 0) {
            auto& a = *unverifiedAddresses.begin();
            if (!failedAddresses.contains(a) && !verified.contains(a)) {
                if (pendingOutgoing.emplace(a, now).second)
                    out.push_back(a);
            }
            unverifiedAddresses.erase(unverifiedAddresses.begin());
        } else
            break;
    }

    return out;
}
bool AddressManager::is_own_endpoint(EndpointAddress a)
{
    return (std::find(ownIps.begin(), ownIps.end(), a.ipv4) != ownIps.end()
        && config().node.bind.port == a.port);
}

void AddressManager::insert_unverified(EndpointAddress a)
{
    if (pendingOutgoing.contains(a)
        || verified.contains(a)
        || failedAddresses.contains(a)
        || is_own_endpoint(a)
        || !a.ipv4.is_valid(config().peers.allowLocalhostIp))
        return;

    unverifiedAddresses.insert(a);
}

void AddressManager::queue_verification(EndpointAddress a)
{
    insert_unverified(a);
}

void AddressManager::queue_verification(const std::vector<EndpointAddress>& as)
{
    spdlog::debug("Queueing {} unverified addresses. BEFORE: {}", as.size(), unverifiedAddresses.size());
    for (auto& a : as) {
        insert_unverified(a);
    }
}

void AddressManager::just_verified(EndpointAddress a, bool setTimer)
{
    pendingOutgoing.erase(a);
    peerServer.async_register_peer(a);
    peerServer.async_seen_peer(a);
    spdlog::debug("DB seen peer {}", a.to_string());

    auto now = sc::now();
    auto p = verified.try_emplace(a, timer.end());
    if (setTimer) {
        set_timer(now + successSleep, p.first);
    }
    p.first->second.lastVerified = now;
    if (p.second)
        check_prune_verified();
}

void AddressManager::check_prune_verified()
{
    // prune by connected and lastVerified
    if (verified.size() >= verifiedPruneAt) {
        using VI = decltype(verified)::iterator;
        std::vector<VI> v;
        for (auto iter = verified.begin(); iter != verified.end(); ++iter) {
            v.push_back(iter);
        }
        std::sort(v.begin(), v.end(), [](VI i1, VI i2) -> bool {
            return (i1->second.outboundConnection < i2->second.outboundConnection)
                || (i1->second.lastVerified < i2->second.lastVerified);
        });
        assert(verifiedPruneTo <= verifiedPruneAt);
        const size_t N = verified.size() - verifiedPruneTo;
        for (size_t i = 0; i < N; ++i) {
            remove_timer(v[i]);
            verified.erase(v[i]);
        }
    }
}

void AddressManager::remove_timer(decltype(verified)::iterator iter)
{
    auto& timer_iter = iter->second.timer_iter;
    if (timer_iter == timer.end())
        return;
    timer.erase(timer_iter);
    timer_iter = timer.end();
}

void AddressManager::set_timer(sc::time_point expires, decltype(verified)::iterator iter)
{
    remove_timer(iter);
    auto timer_iter = timer.emplace(expires, iter);
    iter->second.timer_iter = timer_iter;
}
}
