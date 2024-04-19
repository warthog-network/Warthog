#include "address_manager.hpp"
#include "transport/tcp/connection.hpp"
#include "global/globals.hpp"
#include <algorithm>
#include <future>
#include <random>

namespace address_manager {

AddressManager::AddressManager(PeerServer& peerServer, const std::vector<Sockaddr>& v)
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

void AddressManager::verify(std::vector<Sockaddr> v, IPv4 source)
{
    for (auto& ea : v)
        connectionSchedule.insert(ea, source);
}

std::optional<Conref> AddressManager::find(uint64_t id)
{
    auto iter = conndatamap.find(id);
    if (iter == conndatamap.end())
        return {};
    return ConrefIter { iter };
}

auto AddressManager::prepare_insert(const std::shared_ptr<ConnectionBase>& c) -> tl::expected<std::optional<EvictionCandidate>, int32_t>
{
    auto ip { c->connection_peer_addr().ipv4() };
    if (!ip.is_localhost()) {
        if (ipCounter.contains(ip))
            return tl::unexpected(EDUPLICATECONNECTION);
        if (!c->inbound()) {

            c->successfulConnection = true;
            connectionSchedule.connection_established(*c);

            insert_additional_verified(c->connection_peer_addr()); // TODO: additional_verified necessary?
        } else
            connectionSchedule.insert(c->claimed_peer_addr(), c->connection_peer_addr().ipv4());
    }
    return eviction_candidate();
}

Conref AddressManager::insert_prepared(const std::shared_ptr<ConnectionBase>& c, HeaderDownload::Downloader& h, BlockDownload::Downloader& b, Timer& t)
{

    // insert int conndatamap
    auto p = conndatamap.try_emplace(c->id, c, h, b, t);
    assert(p.second);
    Conref cr { p.first };
    c->dataiter = cr.iterator();

    auto ip { c->connection_peer_addr().ipv4() };
    assert(ip.is_localhost() || ipCounter.insert(ip, 1) == 1);

    if (c->inbound()) {
        inboundConnections.push_back(cr);
    } else {
        outboundEndpoints.push_back(c->connection_peer_addr());
    }
    return cr;
}

bool AddressManager::erase(Conref cr)
{
    ipCounter.erase(cr.peer().ipv4());
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
    for (auto& r : popped) {
        std::visit([&](const auto& address) {
            start_connection(make_request(address, r.sleptFor));
        },
            r.address.data);
    }
}

std::optional<std::chrono::steady_clock::time_point> AddressManager::pop_scheduled_connect_time()
{
    return connectionSchedule.pop_wakeup_time();
}

void AddressManager::start_connection(const TCPConnectRequest& r)
{
    global().conman->connect(r);
}

void AddressManager::insert_additional_verified(Sockaddr newAddress)
{
    std::erase_if(additionalEndpoints, [&](Sockaddr& ea) -> bool {
        return ea.ipv4() == newAddress.ipv4();
    });
    if (ipCounter.contains(newAddress.ipv4())) {
        for (auto& ea : outboundEndpoints) {
            if (ea.ipv4() == newAddress.ipv4()) {
                ea = newAddress;
                return;
            }
        }
        outboundEndpoints.push_back(newAddress);
        return;
    } else {
        additionalEndpoints.push_back(newAddress);
        if (additionalEndpoints.size() > 1100) {
            additionalEndpoints.erase(additionalEndpoints.begin(), additionalEndpoints.begin() + 100);
        }
    }
}

template <typename T>
[[nodiscard]] static auto sample_from_vec(const std::vector<T>& v, size_t N)
{
    std::vector<std::remove_cv_t<T>> out;
    std::sample(v.begin(), v.end(), std::back_inserter(out),
        N, std::mt19937 { std::random_device {}() });
    return out;
}

auto AddressManager::eviction_candidate() const -> std::optional<EvictionCandidate>
{
    if (conndatamap.size() < 200)
        return {};
    auto sampled { sample_from_vec(inboundConnections, 1) };
    assert(sampled.size() == 1);
    return EvictionCandidate { sampled[0] };
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
