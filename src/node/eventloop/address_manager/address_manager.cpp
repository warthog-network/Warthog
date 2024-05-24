#include "address_manager.hpp"
#include "global/globals.hpp"
#include "transport/helpers/start_connection.hpp"
#include "transport/tcp/connection.hpp"
#include <algorithm>
#include <future>
#include <random>

namespace address_manager {

AddressManager::AddressManager(PeerServer& peerServer, const std::vector<TCPSockaddr>& v)
    : peerServer(peerServer)
    , connectionSchedule(peerServer, v)
{
}

void AddressManager::start()
{
    connectionSchedule.start();
    start_scheduled_connections();
};

void AddressManager::outbound_failed(const ConnectRequest& r)
{
    connectionSchedule.outbound_failed(r);
}

void AddressManager::verify(std::vector<TCPSockaddr> v, IPv4 source)
{
    for (auto& ea : v)
        connectionSchedule.insert(ea, source);
}

std::optional<Conref> AddressManager::find(uint64_t id) const
{
    auto iter = conndatamap.find(id);
    if (iter == conndatamap.end())
        return {};
    return ConrefIter { iter };
}

auto AddressManager::insert(InsertData id) -> tl::expected<Conref, int32_t>
{
    auto c { id.convar.base() };
    auto ip { c->peer_addr().ip() };
    if (!ip.is_localhost()) {
        if (ipCounter.contains(ip))
            return tl::unexpected(EDUPLICATECONNECTION);
        if (id.convar.is_tcp()) {
            auto& tcp_con { id.convar.get_tcp() };
            auto ipv4 { tcp_con->peer_ipv4() };
            if (!c->inbound()) {
                c->successfulConnection = true;
                connectionSchedule.connection_established(*tcp_con);
                // insert_additional_verified(c->connection_peer_addr()); // TODO: additional_verified necessary?
            } else
                connectionSchedule.insert(tcp_con->claimed_peer_addr(), ipv4);
        }
    }

    if (auto c { eviction_candidate() })
        id.evict_cb(*c);

    // insert into conndatamap
    auto p = conndatamap.try_emplace(c->id, c->get_shared(), id.headerDownload, id.blockDownload, id.timer);
    assert(p.second);
    Conref cr { p.first };
    c->dataiter = cr.iterator();

    assert(ip.is_localhost() || ipCounter.insert(ip, 1) == 1);

    if (c->inbound()) {
        inboundConnections.push_back(cr);
    } else {
        outboundEndpoints.push_back(c->peer_addr());
    }
    return cr;
}

// auto AddressManager::prepare_insert(const ConnectionBase::ConnectionVariant& c) -> tl::expected<std::optional<Conref>, int32_t>
// {
//     auto ip { c->connection_ipv4() };
//     if (!ip.is_localhost()) {
//         if (ipCounter.contains(ip))
//             return tl::unexpected(EDUPLICATECONNECTION);
//         if (!c->inbound()) {
//
//             c->successfulConnection = true;
//             connectionSchedule.connection_established(*c);
//
//             // insert_additional_verified(c->connection_peer_addr()); // TODO: additional_verified necessary?
//         } else
//             connectionSchedule.insert(c->claimed_peer_addr(), ip);
//     }
//     return eviction_candidate();
// }

// auto AddressManager::prepare_insert(const std::shared_ptr<TCPConnection>& c)
//     -> tl::expected<std::optional<Conref>, int32_t>
// {
//     auto ip { c->connection_ipv4() };
//     if (!ip.is_localhost()) {
//         if (ipCounter.contains(ip))
//             return tl::unexpected(EDUPLICATECONNECTION);
//         if (!c->inbound()) {
//
//             c->successfulConnection = true;
//             connectionSchedule.connection_established(*c);
//
//             // insert_additional_verified(c->connection_peer_addr()); // TODO: additional_verified necessary?
//         } else
//             connectionSchedule.insert(c->claimed_peer_addr(), ip);
//     }
//     return eviction_candidate();
// }
// auto AddressManager::prepare_insert(const std::shared_ptr<WSConnection>&)
//     -> tl::expected<std::optional<Conref>, int32_t>
// {
// }
//
// auto AddressManager::prepare_insert(const std::shared_ptr<RTCConnection>&)
//     -> tl::expected<std::optional<Conref>, int32_t>
// {
// }

// Conref AddressManager::insert_prepared(const std::shared_ptr<ConnectionBase>& c, HeaderDownload::Downloader& h, BlockDownload::Downloader& b, Timer& t)
// {
//
//     // insert int conndatamap
//     auto p = conndatamap.try_emplace(c->id, c, h, b, t);
//     assert(p.second);
//     Conref cr { p.first };
//     c->dataiter = cr.iterator();
//
//     auto ip { c->connection_peer_addr().ip() };
//     assert(ip.is_localhost() || ipCounter.insert(ip, 1) == 1);
//
//     if (c->inbound()) {
//         inboundConnections.push_back(cr);
//     } else {
//         outboundEndpoints.push_back(c->connection_peer_addr());
//     }
//     return cr;
// }

bool AddressManager::erase(Conref cr)
{
    ipCounter.erase(cr.peer().ip());
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

void AddressManager::start_scheduled_connections()
{
    auto popped { connectionSchedule.pop_expired() };
    for (auto& r : popped)
        start_connection(make_request(r.address, r.sleptFor));
}

std::optional<std::chrono::steady_clock::time_point> AddressManager::pop_scheduled_connect_time()
{
    return connectionSchedule.pop_wakeup_time();
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

// namespace {
//     // Peerserver: unpin, pin
//     //async_get_recent_peers
//     // own_ips
//     //
//     // verify queue
//     auto interface_ips_v4() // TODO: move to peerserver
//     {
//         std::vector<IPv4> out;
//         uv_interface_address_t* info;
//         int count;
//         uv_interface_addresses(&info, &count);
//         for (int i = 0; i < count; ++i) {
//             uv_interface_address_t interface_a = info[i];
//             if (interface_a.address.address4.sin_family == AF_INET) {
//                 out.push_back(interface_a.address.address4);
//             }
//         }
//         uv_free_interface_addresses(info, count);
//         return out;
//     }
// }
//
// bool AddressManager::is_own_endpoint(EndpointAddress a)
// {
//     return (std::find(ownIps.begin(), ownIps.end(), a.ipv4) != ownIps.end()
//         && config().node.bind.port == a.port);
// }

// void AddressManager::insert_unverified(EndpointAddress a)
// {
//     if (pendingOutgoing.contains(a)
//         || verified.contains(a)
//         || failedAddresses.contains(a)
//         || is_own_endpoint(a)
//         || !a.ipv4.is_valid(config().peers.allowLocalhostIp))
//         return;
//
//     unverifiedAddresses.insert(a);
// }
}
