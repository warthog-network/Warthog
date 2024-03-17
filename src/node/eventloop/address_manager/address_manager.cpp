#include "address_manager.hpp"
#include "asyncio/connection.hpp"
#include "global/globals.hpp"
#include <algorithm>
#include <future>
#include <random>

namespace address_manager {

std::optional<Conref> AddressManager::find(uint64_t id)
{
    auto iter = conndatamap.find(id);
    if (iter == conndatamap.end())
        return {};
    return ConrefIter { iter };
}

auto AddressManager::prepare_insert(const std::shared_ptr<ConnectionBase>& c) -> tl::expected<EvictionCandidate, int32_t>
{
    auto ip { c->peer().ipv4 };
    if (!ip.is_localhost()) {
        if (ipCounter.contains(ip))
            return tl::unexpected(EDUPLICATECONNECTION);
        if (!c->inbound()) {
            global().peerServer->notify_successful_outbound(c);
            insert_additional_verified(c->peer());
        } else
            global().peerServer->verify_peer(c->peer_endpoint(), c->peer().ipv4);
    }
    auto ec { eviction_candidate() };
    if (!ec.has_value())
        return tl::unexpected(EFAILEDEVICTION);
    return *ec;
}

Conref AddressManager::insert_prepared(const std::shared_ptr<ConnectionBase>& c, HeaderDownload::Downloader& h, BlockDownload::Downloader& b, Timer& t)
{

    // insert int conndatamap
    auto p = conndatamap.try_emplace(c->id, c, h, b, t);
    assert(p.second);
    Conref cr { p.first };
    c->dataiter = cr.iterator();

    auto ip { c->peer().ipv4 };
    assert(ip.is_localhost() || ipCounter.insert(ip, 1) == 1);

    if (c->inbound()) {
        inboundConnections.push_back(cr);
    } else {
        outboundEndpoints.push_back(c->peer());
    }
    return cr;
}

bool AddressManager::erase(Conref cr)
{
    ipCounter.erase(cr.peer().ipv4);
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

void AddressManager::insert_additional_verified(EndpointAddress newAddress)
{
    std::erase_if(additionalEndpoints, [&](EndpointAddress& ea) -> bool {
        return ea.ipv4 == newAddress.ipv4;
    });
    if (ipCounter.contains(newAddress.ipv4)) {
        for (auto& ea : outboundEndpoints) {
            if (ea.ipv4 == newAddress.ipv4) {
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
