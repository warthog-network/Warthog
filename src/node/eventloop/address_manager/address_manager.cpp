#include "address_manager.hpp"
#include "global/globals.hpp"
#ifndef DISABLE_LIBUV
#include "transport/tcp/connection.hpp"
#endif
#include <algorithm>
#include <future>
#include <random>

AddressManager::OutboundClosedEvent::OutboundClosedEvent(std::shared_ptr<ConnectionBase> c, Error reason)
    : c(std::move(c))
    , reason(reason)
{
    assert(!this->c->inbound());
}

#ifndef DISABLE_LIBUV
AddressManager::AddressManager(InitArg ia)
    : tcpConnectionSchedule(std::move(ia))
#else
AddressManager::AddressManager(InitArg ia)
    : wsConnectionSchedule(std::move(ia))
#endif
{
}

void AddressManager::start()
{
#ifndef DISABLE_LIBUV
    tcpConnectionSchedule.start();
#endif
    start_scheduled_connections();
};

void AddressManager::outbound_closed(OutboundClosedEvent e)
{
    bool success { e.c->successfulConnection };
    auto reason { e.reason };
    if (auto cr { e.c->connect_request() }) {
#ifndef DISABLE_LIBUV
        tcpConnectionSchedule.outbound_closed(*cr, success, reason);
#else
        wsConnectionSchedule.outbound_closed(*cr, success, reason);
#endif
    }
}

void AddressManager::start_scheduled_connections()
{
    if (config().node.isolated)
        return;
#ifndef DISABLE_LIBUV
    tcpConnectionSchedule.connect_expired();
#else
    wsConnectionSchedule.connect_expired();
#endif
}

#ifndef DISABLE_LIBUV
void AddressManager::verify(std::vector<TCPPeeraddr> v, IPv4 source)
{
    for (auto& ea : v)
        tcpConnectionSchedule.insert(ea, source);
}

void AddressManager::outbound_failed(const TCPConnectRequest& r)
{
    tcpConnectionSchedule.outbound_failed(r);
}

#else

void AddressManager::outbound_failed(const WSBrowserConnectRequest& r)
{
    wsConnectionSchedule.outbound_failed(r);
}
#endif

std::optional<Conref> AddressManager::find(uint64_t id) const
{
    auto iter = conndatamap.find(id);
    if (iter == conndatamap.end())
        return {};
    return ConrefIter { iter };
}

auto AddressManager::insert(InsertData id) -> tl::expected<Conref, Error>
{
    auto c { id.convar.base() };
    auto ip { c->peer_addr().ip() };
    if (ip && !ip->is_loopback()) {
        if (ipCounter.contains(*ip))
            return tl::unexpected(EDUPLICATECONNECTION);
        if (!c->inbound())
            c->successfulConnection = true;
#ifndef DISABLE_LIBUV
        if (id.convar.is_tcp()) {
            auto& tcp_con { id.convar.get_tcp() };
            auto ipv4 { tcp_con->peer_addr_native().ip };
            if (!c->inbound()) {
                tcpConnectionSchedule.outbound_established(*tcp_con);
                // insert_additional_verified(c->connection_peer_addr()); // TODO: additional_verified necessary?
            } else
                tcpConnectionSchedule.insert(tcp_con->claimed_peer_addr(), ipv4);
        }
#endif
    }

    if (auto c { eviction_candidate() })
        id.evict_cb(*c);

    // insert into conndatamap
    auto p = conndatamap.try_emplace(c->id, c->get_shared(), id.headerDownload, id.blockDownload, id.timer);
    assert(p.second);
    Conref cr { p.first };
    c->dataiter = cr.iterator();

    assert(!ip || ip->is_loopback() || ipCounter.insert(*ip, 1) == 1);

    if (c->inbound()) {
        inboundConnections.push_back(cr);
    } else {
        outboundEndpoints.push_back(c->peer_addr());
    }
    return cr;
}

bool AddressManager::erase(Conref cr)
{
    auto ip { cr.peer().ip() };
    if (ip)
        ipCounter.erase(*ip);
    if (cr->c->inbound()) {
        std::erase(inboundConnections, cr);
    } else {
        std::erase(outboundEndpoints, cr.peer());
    }
    delayedDelete.push_back(cr);
    return false;
}

void AddressManager::garbage_collect()
{
    for (auto cr : delayedDelete) {
        conndatamap.erase(cr.iterator());
    }
    delayedDelete.clear();
}

std::optional<std::chrono::steady_clock::time_point> AddressManager::pop_scheduled_connect_time()
{
    if (config().node.isolated)
        return {};
#ifndef DISABLE_LIBUV
    return tcpConnectionSchedule.pop_wakeup_time();
#else
    return wsConnectionSchedule.pop_wakeup_time();
#endif
}

template <typename T>
[[nodiscard]] static auto sample_from_vec(const std::vector<T>& v, size_t N)
{
    std::vector<std::remove_cv_t<T>> out;
    std::sample(v.begin(), v.end(), std::back_inserter(out),
        N, std::mt19937 { std::random_device {}() });
    return out;
}

auto AddressManager::eviction_candidate() const -> std::optional<Conref>
{
    if (conndatamap.size() < 200)
        return {};
    auto sampled { sample_from_vec(inboundConnections, 1) };
    assert(sampled.size() == 1);
    return Conref { sampled[0] };
}
