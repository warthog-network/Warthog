#include "connection_schedule.hxx"
// #include "global/globals.hpp"
#include "peerserver/peerserver.hpp"
#include "spdlog/spdlog.h"
#include "transport/connect_request.hpp"
#ifndef DISABLE_LIBUV
#include "transport/tcp/connection.hpp"
#endif
#include <cassert>
#include <functional>
#include <future>
#include <random>
#include <type_traits>

using namespace std::chrono;
using namespace std::chrono_literals;

using sc = std::chrono::steady_clock;

namespace connection_schedule {

size_t ConnectionLog::consecutive_failures() const
{
    size_t z(std::countr_zero(bits >> 5));
    size_t a { active_bits() };
    return std::min(z, a);
}

bool ConnectionLog::last_connection_failed() const
{
    return (bits & (1 << 5)) == 0;
}

void ConnectionLog::log_failure()
{
    uint32_t active { active_bits() + 1 };
    if ((active >> 5) > 0) {
        active = 0x0000001Fu;
    }
    const auto logbits { bits >> 5 };
    bits = (logbits << 6) | active;
}

void ConnectionLog::log_success()
{
    uint32_t active { active_bits() + 1 };
    if ((active >> 5) > 0) {
        active = 0x0000001Fu;
    }
    bits = (((bits >> 4) | 0x00000001) << 5) | active;
}

void VectorEntryBase::add_source(Source s)
{
    sources.insert(s);
}

template <typename addr_t>
std::optional<time_point> VectorEntry<addr_t>::try_pop(time_point now, std::vector<ConnectRequest>& out)
{
    if (pending == true)
        return {};
    if (!timer.expired(now))
        return timer.timeout();

    pending = true;
    out.push_back({ address, timer.sleep_duration() });
    return {};
}

std::optional<time_point> VectorEntryBase::timeout() const
{
    if (pending == true)
        return {};
    return timer.timeout();
}

void VectorEntryBase::connection_established()
{
    pending = false;
    connected += 1;
    connectionLog.log_success();
}

time_point VectorEntryBase::outbound_connected_ended(const ReconnectContext& c)
{
    assert(pending);
    pending = false;
    if (c.connectionState == ConnectionState::NOT_CONNECTED) {
        connectionLog.log_failure();
    } else {
        assert(connected > 0);
        connected -= 1;
        if (c.connectionState == ConnectionState::UNINITIALIZED)
            connectionLog.log_failure();
    }
    return update_timer(c);
}

time_point VectorEntryBase::update_timer(const ReconnectContext& c)
{
    const bool verified { c.endpointState == SockaddrState::VERIFIED };
    auto consecutiveFailures { connectionLog.consecutive_failures() };
    auto wait = std::invoke([&]() -> duration {
        // if everything went well, plan do regular check of peer
        if (consecutiveFailures == 0 && verified)
            return 5min;

        // first failure
        if (consecutiveFailures == 1) {
            if (verified || c.pinned) {
                return 1s; // immediately retry
            } else {
                // unverified failed connections' first retry is in 30s
                return 30s;
            }
        }

        // increase timer duration for failed
        auto d { c.prevWait };
        if (d < 1s)
            d = 1s;
        else
            d *= 2;

        if (c.pinned)
            return std::min(d, duration { 20s });
        else
            return std::min(d, duration { 30min });
    });
    timer.set(wait);
    return timer.timeout();
}


template <typename addr_t>
std::pair<VectorEntry<addr_t>&, bool> VerifiedVector<addr_t>::emplace(const WithSource<addr_t>& i, tp lastVerified)
{
    auto p { this->find(i.address) };
    if (p)
        return { *p, false };
    VectorEntry<addr_t>& e { this->insert(VerifiedEntry<addr_t> { i, lastVerified }) };
    if (auto t { e.timeout() }; t)
        this->update_wakeup_time(*t);
    return { e, true };
}

template <typename EntryData, typename addr_t>
auto SockaddrVectorBase<EntryData, addr_t>::push_back(elem_t e) -> elem_t&
{
    data.push_back(std::move(e));
    return data.back();
}

template <typename EntryData, typename addr_t>
void SockaddrVectorBase<EntryData, addr_t>::pop_requests(time_point now, std::vector<ConnectRequest>& out)
{
    if (!wakeup_tp || wakeup_tp > now)
        return;
    wakeup_tp.reset();
    for (auto& e : data)
        update_wakeup_time(e.try_pop(now, out));
}

template <typename EntryData, typename addr_t>
VectorEntry<addr_t>& SockaddrVectorBase<EntryData, addr_t>::insert(elem_t&& ed)
{
    return data.emplace_back(ed);
}

template <typename EntryData, typename addr_t>
void SockaddrVectorBase<EntryData, addr_t>::update_wakeup_time(const std::optional<time_point>& tp)
{
    if (wakeup_tp < tp)
        wakeup_tp = tp;
}

template <typename EntryData, typename addr_t>
EntryData* SockaddrVectorBase<EntryData, addr_t>::find(const addr_t& address) const
{
    auto iter { std::find_if(data.begin(), data.end(), [&](auto& elem) { return elem.address == address; }) };
    if (iter == data.end())
        return nullptr;
    return &*iter;
}
}

ConnectionSchedule::ConnectionSchedule(PeerServer& peerServer, const std::vector<TCPSockaddr>& pin)
    : pinned(pin.begin(), pin.end())
    , peerServer(peerServer)
{
    spdlog::info("Peers connect size {} ", pin.size());
    for (auto& p : pinned)
        pinned.insert(p);
}

auto ConnectionSchedule::find_verified(const TCPSockaddr& sa) -> VectorEntryBase*
{
    return verified.find(sa);
}

auto ConnectionSchedule::find(const TCPSockaddr& a) const -> std::optional<Found>
{
    using enum EndpointState;
    VectorEntryBase* p{verified.find(a)};
    if (p)
        return Found { *p, VERIFIED };
    if (p = unverifiedNew.find(a); p)
        return Found { *p, UNVERIFIED_NEW };
    if (p = unverifiedFailed.find(a); p)
        return Found { *p, UNVERIFIED_FAILED };
    return {};
}


// auto ConnectionSchedule::invoke_with_verified(const TCPSockaddr& a, auto lambda)
// {
//     return std::visit(
//         [&]<typename T>(const T& addr) {
//             return lambda(addr, verified);
//         },
//         a.data);
// }
//
// auto ConnectionSchedule::invoke_with_verified(const TCPSockaddr& a, auto lambda) const
// {
//     return std::visit(
//         [&]<typename T>(const T& addr) {
//             return lambda(addr, verified.get<std::remove_cvref_t<T>>());
//         },
//         a.data);
// }

#ifndef DISABLE_LIBUV
auto ConnectionSchedule::emplace_verified(const WithSource<TCPSockaddr>& s, steady_clock::time_point lastVerified)
{
    return verified.emplace({s.address,s.source},lastVerified).second;
}




std::optional<ConnectRequest> ConnectionSchedule::insert(TCPSockaddr addr, Source src)
{
    auto o { find(addr) };
    if (o.has_value()) {
        // only track sources of addresses that are not verified
        if (o->state != EndpointState::VERIFIED)
            o->item.add_source(src);
        return {};
    } else {
        unverifiedNew.emplace({ addr, src }); // TODO: check if unverified is cleared at some point
        wakeup_tp.consider(unverifiedNew.timeout());
        return ConnectRequest { addr, 0s };
    }
}

auto ConnectionSchedule::move_entry(SockaddrVector& ev, const TCPSockaddr& addr) -> VectorEntryBase*
{

    using elem_t = SockaddrVector::elem_t;
    VectorEntryBase* elem = nullptr;
    ev.erase(addr, [&](elem_t&& deleted) {
            elem = &verified.push_back({ std::move(deleted), addr, sc::now() });
    });
    return elem;
}

void ConnectionSchedule::connection_established(const TCPConnection& c)
{ // OK
  
    if (c.inbound())
        return;
    const TCPSockaddr& ea { c.peer_addr_native() };
    VectorEntryBase* p { move_entry(unverifiedNew, ea) };
    if (!p)
        p = move_entry(unverifiedFailed, ea);
    if (!p)
        p = find_verified(ea);
    if (!p)
        return;
    p->connection_established();
}
namespace connection_schedule {
void SockaddrVector::erase(const TCPSockaddr& a, auto lambda)
{
    std::erase_if(data, [&a, &lambda](elem_t& d) {
        if (d.address == a) {
            lambda(std::move(d));
            return true;
        }
        return false;
    });
}

auto SockaddrVector::emplace(const WithSource<TCPSockaddr>& i) -> std::pair<elem_t&, bool>
{
    auto p { find(i.address) };
    if (p)
        return { *p, false };
    elem_t& e { insert(elem_t { i }) };
    if (auto t { e.timeout() }; t)
        update_wakeup_time(*t);
    return { e, true };
}
}
#endif

void ConnectionSchedule::start()
{
    constexpr size_t maxRecent = 100;
    constexpr connection_schedule::Source startup_source { 0 };

    // get recently seen peers from db
    std::promise<std::vector<std::pair<TCPSockaddr, uint32_t>>> p;
    auto future { p.get_future() };
    auto cb = [&p](std::vector<std::pair<TCPSockaddr, uint32_t>>&& v) {
        p.set_value(std::move(v));
    };
    peerServer.async_get_recent_peers(std::move(cb), maxRecent);

    auto db_peers = future.get();
    int64_t nowts = now_timestamp();
#ifndef DISABLE_LIBUV
    for (const auto& [a, timestamp] : db_peers) {
        auto lastVerified = sc::now() - seconds((nowts - int64_t(timestamp)));
        auto wasInserted { emplace_verified({ a, startup_source }, lastVerified) };
        assert(wasInserted);
    }
#endif
};

void ConnectionSchedule::outbound_closed(const peerserver::ConnectionData& c)
{
    using enum ConnectionState;
    auto state { c.successfulConnection ? INITIALIZED : UNINITIALIZED };
    if (auto r{c.connect_request()}) 
        outbound_connection_ended(*r, state);

    // TODO: make sure prune does not discard pending entries

    // reconnect?
    // * reconnect immediately if pinned
    // * outbound connect later if evil disconnect reason
    // * outbound connect immediately if different reason
    // * outbound don't connect if disconnected on purpose due to too many connections
}

void ConnectionSchedule::outbound_failed(const ConnectRequest& cr)
{
    outbound_connection_ended(cr, ConnectionState::NOT_CONNECTED);
}

auto ConnectionSchedule::pop_wakeup_time() -> std::optional<time_point>
{
    return wakeup_tp.pop();
}

void ConnectionSchedule::outbound_connection_ended(const ConnectRequest& r, ConnectionState state)
{
    if (auto o { get_context(r, state) })
        wakeup_tp.consider(o->item.outbound_connected_ended(o->context));
}

std::vector<ConnectRequest> ConnectionSchedule::pop_expired()
{
    auto now { steady_clock::now() };
    if (!wakeup_tp.expired())
        return {};

    // pop expired requests
    std::vector<ConnectRequest> res;
    verified.pop_requests(now, res);
    unverifiedNew.pop_requests(now, res);
    unverifiedFailed.pop_requests(now, res);

    refresh_wakeup_time();
    return res;
}

void ConnectionSchedule::refresh_wakeup_time()
{
    wakeup_tp.reset();
    wakeup_tp.consider(verified.timeout());
    wakeup_tp.consider(unverifiedNew.timeout());
    wakeup_tp.consider(unverifiedFailed.timeout());
}

auto ConnectionSchedule::get_context(const ConnectRequest& r, ConnectionState cs) -> std::optional<FoundContext>
{
    if (auto p { find(r.address) }; p) {
        if (cs == ConnectionState::INITIALIZED)
            assert(p->state == EndpointState::VERIFIED);

        return FoundContext {
            p->item,
            ReconnectContext {
                .prevWait { r.sleptFor },
                .endpointState = p->state,
                .connectionState = cs,
                .pinned = pinned.contains(r.address) }
        };
    }
    return {};
};
