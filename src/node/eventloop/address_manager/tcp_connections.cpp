#include "tcp_connections.hpp"
#include "peerserver/peerserver.hpp"
#include "spdlog/spdlog.h"
#include "general/errors.hpp"
#include "transport/connect_request.hpp"
#include "transport/tcp/connection.hpp"
#include <algorithm>
#include <cassert>
#include <functional>
#include <future>
#include <nlohmann/json.hpp>
#include <random>
#include <type_traits>

using namespace std::chrono;
using namespace std::chrono_literals;

using sc = std::chrono::steady_clock;
namespace {
auto seconds_from_now(sc::time_point tp)
{
    return duration_cast<seconds>(tp - sc::now()).count();
}

template <class _PopulationIterator,
    class _PopulationSentinel,
    class _SampleIterator,
    class _Distance,
    class _UniformRandomNumberGenerator>
_SampleIterator sample2ranges(
    _PopulationIterator __first,
    _PopulationSentinel __last,
    _PopulationIterator __first2,
    _PopulationSentinel __last2,
    _SampleIterator __output_iter,
    _Distance __n,
    _UniformRandomNumberGenerator&& __g)
{
    _Distance __unsampled_sz = std::distance(__first, __last)
        + std::distance(__first2, __last2);
    for (__n = std::min(__n, __unsampled_sz); __n != 0; ++__first) {
        if (__first == __last) {
            __first = __first2;
            __last = __last2;
        }
        _Distance __r = std::uniform_int_distribution<_Distance>(0, --__unsampled_sz)(__g);
        if (__r < __n) {
            *__output_iter++ = *__first;
            --__n;
        }
    }
    return __output_iter;
}
}

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

void VectorEntry::add_source(Source s)
{
    sources.insert(s);
}

void VectorEntry::log_success()
{
    connectionLog.log_success();
}

void VectorEntry::log_failure()
{
    connectionLog.log_failure();
}

json VectorEntry::to_json() const
{
    return {
        { "address", address.to_string() },
        { "lastError", lastError.is_error() ? json(lastError.format()) : json(nullptr) }
    };
}

json EntryWithTimer::Timer::to_json() const
{
    using namespace std::chrono;
    return {
        { "sleepDuration", duration_cast<seconds>(_sleepDuration).count() },
        { "expiresIn", wakeupTime ? json(seconds_from_now(*wakeupTime)) : json(nullptr) }
    };
}

void EntryWithTimer::wakeup_after(duration d)
{
    timer = Timer(d);
}

json EntryWithTimer::to_json() const
{
    auto j(VectorEntry::to_json());
    j["timer"] = timer.to_json();
    return j;
}

// time_point EntryWithTimer::outbound_connected_ended(const ReconnectContext& c)
// {
//     assert(!timer.has_value());
//     using enum ConnectionState;
//     switch (c.connectionState) {
//     case NOT_CONNECTED:
//     case CONNECTED_UNINITIALIZED:
//         connectionLog.log_failure();
//         break;
//     case CONNECTED_INITIALIZED:
//         break;
//     }
//     return update_timer(c);
// }

std::optional<time_point> EntryWithTimer::wakeup_time() const
{
    return timer.wakeup_time();
}

std::optional<time_point> EntryWithTimer::make_expired_pending(time_point now, std::vector<ConnectRequest>& outpending)
{
    if (!timer.active())
        return {};
    if (!timer.expired_at(now))
        return timer.wakeup_time();
    outpending.push_back(TCPConnectRequest::make_outbound(address, timer.sleep_duration()));
    timer.deactivate();
    return {};
}

// time_point EntryWithTimer::update_timer(const ReconnectContext& c)
// {
//     const bool verified { c.endpointState == VerificationState::VERIFIED };
//     auto consecutiveFailures { connectionLog.consecutive_failures() };
//     auto wait = std::invoke([&]() -> duration {
//         // if everything went well, plan do regular check of peer
//         if (consecutiveFailures == 0 && verified)
//             return 5min;
//
//         // first failure
//         if (consecutiveFailures == 1) {
//             if (verified || c.pinned) {
//                 return 1s; // immediately retry
//             } else {
//                 // unverified failed connections' first retry is in 30s
//                 return 30s;
//             }
//         }
//
//         // increase timer duration for failed
//         auto d { c.prevWait };
//         if (d < 1s)
//             d = 1s;
//         else
//             d *= 2;
//
//         if (c.pinned)
//             return std::min(d, duration { 20s });
//         else
//             return std::min(d, duration { 30min });
//     });
//     timer = Timer(wait);
//     return timer->wakeup_time();
// }

std::pair<VectorEntry&, bool> VerifiedVector::insert(const TCPWithSource& i, tp lastVerified)
{
    auto p { this->find(i.address) };
    if (p)
        return { *p, false };
    auto& e { this->push_back(VerifiedEntry { i, lastVerified }) };
    update_wakeup_time(e.wakeup_time());
    return { e, true };
}

void VerifiedVector::prune(auto&& pred, size_t n)
{
    if (data.size() <= n)
        return;
    size_t d { data.size() - n };
    std::erase_if(data, [&](VerifiedEntry& e) {
        return (d-- != 0)
            && e.wakeup_time().has_value() // no pending connection
            && pred(e);
    });
}

json VerifiedEntry::to_json() const
{
    auto json(EntryWithTimer::to_json());
    json["lastVerified"] = seconds_from_now(lastVerified);
    return json;
}

void TimeoutInfo::update_wakeup_time(const std::optional<time_point>& tp)
{
    if (tp && (!wakeup_tp || wakeup_tp > tp))
        wakeup_tp = tp;
}

void FoundDisconnected::wakeup_after(duration d)
{
    match.wakeup_after(d);
    timeout.update_wakeup_time(match.wakeup_time());
}

template <typename T>
size_t SockaddrVectorBase<T>::erase(const TCPPeeraddr& a, auto lambda)
{
    auto iter = std::partition(data.begin(), data.end(), [&](elem_t& e) {
        return e.address != a;
    });
    std::for_each(iter, data.end(), [&](auto& e) { lambda(std::move(e)); });
    auto n { data.end() - iter };
    data.erase(iter, data.end());
    return n;
}

template <typename T>
json SockaddrVectorBase<T>::to_json() const
{
    json j(json::array());
    for (auto& d : data) {
        j.push_back(d.to_json());
    }
    return j;
}

template <typename T>
auto SockaddrVectorBase<T>::push_back(elem_t e) -> elem_t&
{
    data.push_back(std::move(e));
    return data.back();
}

template <typename T>
void SockaddrVectorBase<T>::take_expired(time_point now, std::vector<ConnectRequest>& outpending)
{
    if (!wakeup_tp || wakeup_tp > now)
        return;
    wakeup_tp.reset();
    for (auto& e : data)
        update_wakeup_time(e.make_expired_pending(now, outpending));
}

template <typename T>
auto SockaddrVectorBase<T>::find(const TCPPeeraddr& address) const -> elem_t*
{
    auto iter { std::find_if(data.begin(), data.end(), [&](auto& elem) { return elem.address == address; }) };
    if (iter == data.end())
        return nullptr;
    return &*iter;
}

auto FeelerVector::insert(const EntryWithTimer& e1) -> std::pair<elem_t&, bool>
{
    auto p { find(e1.address) };
    if (p)
        return { *p, false };
    elem_t& e { push_back(e1) };
    if (auto t { e.wakeup_time() }; t)
        update_wakeup_time(*t);
    return { e, true };
}
auto FeelerVector::insert(const WithSource<TCPPeeraddr>& i) -> std::pair<elem_t&, bool>
{
    auto p { find(i.address) };
    if (p)
        return { *p, false };
    elem_t& e { push_back(elem_t { i }) };
    if (auto t { e.wakeup_time() }; t)
        update_wakeup_time(*t);
    return { e, true };
}

std::vector<TCPPeeraddr> TCPConnectionSchedule::sample_verified(size_t N) const
{
    std::vector<TCPPeeraddr> out;
    out.reserve(N);
    sample2ranges(
        connectedVerified.elements().begin(),
        connectedVerified.elements().end(),
        disconnectedVerified.elements().begin(),
        disconnectedVerified.elements().end(),
        std::back_inserter(out),
        N, std::mt19937 { std::random_device {}() });
    return out;
}

TCPConnectionSchedule::TCPConnectionSchedule(InitArg ia)
    : peerServer(ia.peerServer)
    , pinned(ia.pin.begin(), ia.pin.end())
{
    spdlog::info("Pinned {} peers.", ia.pin.size());
}

[[nodiscard]] auto TCPConnectionSchedule::find_disconnected(const TCPPeeraddr& a) -> std::optional<FoundDisconnected>
{
    EntryWithTimer* p = disconnectedVerified.find(a);
    if (p)
        return FoundDisconnected { *p, disconnectedVerified, true };
    if (p = feelers.find(a); p)
        return FoundDisconnected { *p, feelers, false };
    return {};
}
auto TCPConnectionSchedule::find(const TCPPeeraddr& a) -> std::optional<Found>
{
    VectorEntry* p { connectedVerified.find(a) };
    if (p)
        return Found { *p, connectedVerified, true };
    p = disconnectedVerified.find(a);
    if (p)
        return Found { *p, disconnectedVerified, true };
    if (p = feelers.find(a); p)
        return Found { *p, feelers, false };
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

// auto TCPConnectionSchedule::insert_verified(const TCPWithSource& s, steady_clock::time_point lastVerified)
// {
//     return verified.insert(s, lastVerified).second;
// }

std::optional<ConnectRequest> TCPConnectionSchedule::add_feeler(TCPPeeraddr addr, Source src)
{
    if (auto o { find(addr) }) {
        // only track sources of addresses that are not verified
        if (o->verified)
            o->match.add_source(src);
        return {};
    } else {
        feelers.insert({ addr, src });
        wakeup_tp.consider(feelers.timeout());
        return ConnectRequest::make_outbound(addr, 0s);
    }
}

void TCPConnectionSchedule::on_outbound_connected(const TCPConnection& c)
{
    if (c.inbound())
        return;
    const TCPPeeraddr& a { c.peer_addr_native() };
    VerifiedVector::elem_t* p = nullptr;
    feelers.erase(a, [&](auto&& deleted) {
        p = &connectedVerified.push_back({ std::move(deleted), sc::now() });
    });
    if (!p)
        disconnectedVerified.erase(a,
            [&](auto&& deleted) {
                p = &connectedVerified.push_back({ std::move(deleted), sc::now() });
            });
    if (!p)
        p = connectedVerified.find(a);
    if (!p)
        return;
    p->on_connected();
    prune_verified();
}

void TCPConnectionSchedule::pin(const TCPPeeraddr& a)
{
    auto p { pinned.insert(a) };
    if (p.second) { // newly inserted
        insert_freshly_pinned(a);
    }
}

void TCPConnectionSchedule::unpin(const TCPPeeraddr& a)
{
    if (pinned.erase(a) != 0)
        prune_verified();
}

void TCPConnectionSchedule::initialize()
{
    constexpr size_t maxRecent = 100;

    // get recently seen peers from db
    std::promise<std::vector<std::pair<TCPPeeraddr, Timestamp>>> p;
    auto future { p.get_future() };
    auto cb = [&p](std::vector<std::pair<TCPPeeraddr, Timestamp>>&& v) {
        p.set_value(std::move(v));
    };
    peerServer.async_get_recent_peers(std::move(cb), maxRecent);
    auto db_peers { future.get() };

    // load verified addresses
    const int64_t nowts { now_timestamp() };
    constexpr connection_schedule::Source startup_source { 0 };
    for (const auto& [a, timestamp] : db_peers) {
        auto lastVerified = sc::now() - seconds((nowts - int64_t(timestamp.val())));
        auto [_, wasInserted] = disconnectedVerified.insert({ a, startup_source }, lastVerified);
        assert(wasInserted);
    }

    // load pinned addresses
    for (auto& p : pinned)
        insert_freshly_pinned(p);

    refresh_wakeup_time();
};

void TCPConnectionSchedule::on_outbound_disconnected(const TCPConnectRequest& r, Error err, bool established)
{
    auto a { r.address() };
    if (established) { // an established outgoing connection was closed
        auto e { connectedVerified.find(a) };
        e->lastError = err;
        assert(e != nullptr);
        e->on_disconnected();
        if (e->connections == 0) {
            // all outgoing connections were closed,
            // now move entry from connectedVerified to to disconnectedVerified.
            using entry_t = connection_schedule::ConnectedEntry;
            std::vector<entry_t> tmp;
            auto deleted { connectedVerified.erase(a,
                [&](entry_t&& e) { tmp.push_back(std::move(e)); }) };
            assert(deleted == 1 && tmp.size() == 1);
            auto& el { disconnectedVerified.push_back(tmp.front()) };

            connection_schedule::duration dur { [&]() -> connection_schedule::duration {
                if (err.code == EDUPLICATECONNECTION || err.code == EEVICTED) {
                    // we wanted to close the connection
                    return 10min;
                } else if (pinned.contains(a) || !err.triggers_ban()) {
                    return 0s; // set timer to reconnect immediately
                } else {
                    return seconds(err.bantime());
                }
            }() };

            el.wakeup_after(dur);
            disconnectedVerified.update_wakeup_time(el.wakeup_time());
        }
    } else { // non-established outgoing connection was closed
        on_outbound_failed(r, err);
    }
}

void TCPConnectionSchedule::on_outbound_failed(const TCPConnectRequest& cr, Error err)
{
    auto a { cr.address() };

    auto increas_sleeptime { [&](EntryWithTimer& e) {
        auto d { e.sleep_duration() };
        if (d < 200ms) {
            d = 200ms;
        } else if (d < 1min)
            d *= 2; // exponential backoff
        e.wakeup_after(d);
        feelers.update_wakeup_time(e.wakeup_time());
        wakeup_tp.consider(feelers.wakeup_tp);
    } };
    if (auto f { feelers.find(a) }) {
        if (pinned.contains(a)) { // cannot delete pinned feelers
            f->lastError = err;
            increas_sleeptime(*f);
        } else { // delete from feelers
            assert(feelers.erase(a) == 1);
            return;
        }
    } else { // move entry from disconnectedVerified to feelers
        auto n { disconnectedVerified.erase(a, [&](VerifiedEntry&& e) {
            auto [elem, inserted] { feelers.insert(std::move(e)) };
            elem.lastError = err;
            assert(inserted);
            increas_sleeptime(elem);
        }) };
        assert(n <= 1);
    }
}

void TCPConnectionSchedule::on_inbound_disconnected(const IPv4& ip)
{
    auto [begin, end] { pinned.equal_range(ip) };
    for (auto iter = begin; iter != end; ++iter) {
        auto& addr { *iter };
        auto found { find(addr) };
        assert(found.has_value()); // pinned should always be kept in the list
        // found->timeout.wakeup_tp
    }
}

auto TCPConnectionSchedule::to_json() const -> json
{
    return {
        { "connectedVerified", connectedVerified.to_json() },
        { "disconnectedVerified", disconnectedVerified.to_json() },
        { "feelers", feelers.to_json() },
    };
}

auto TCPConnectionSchedule::updated_wakeup_time() -> std::optional<time_point>
{
    return wakeup_tp.pop();
}

// void TCPConnectionSchedule::outbound_connection_ended(const ConnectRequest& r, ConnectionState conState)
// {
//     // TODO: check wait time logic
//     if (auto o { get_context(r, conState) }) {
//         auto tp { o->match.outbound_connected_ended(o->reconnectInfo) };
//         o->timeout.update_wakeup_time(tp);
//         wakeup_tp.consider(tp);
//     }
//     prune_verified();
// }

void TCPConnectionSchedule::connect_expired()
{
    for (auto& r : pop_expired())
        r.connect();
}

void TCPConnectionSchedule::insert_freshly_pinned(const TCPPeeraddr& a)
{
    if (connectedVerified.find(a))
        return;
    if (auto f { find_disconnected(a) }) {
        f->wakeup_after(0s);
        wakeup_tp.consider(f->timeout.timeout());
    } else {
        constexpr connection_schedule::Source startup_source { 0 };
        feelers.insert({ a, startup_source });
        wakeup_tp.consider(feelers.timeout());
    }
}

void TCPConnectionSchedule::prune_verified()
{
    disconnectedVerified.prune([&](const connection_schedule::VerifiedEntry& e) {
        assert(feelers.insert(e).second);
        return true;
    },
        softboundVerified);
    wakeup_tp.consider(feelers.timeout());
}

std::vector<TCPConnectRequest> TCPConnectionSchedule::pop_expired(time_point now)
{
    if (!wakeup_tp.expired())
        return {};

    // pop expired requests
    std::vector<ConnectRequest> outPending;
    disconnectedVerified.take_expired(now, outPending);
    feelers.take_expired(now, outPending);

    refresh_wakeup_time();
    return outPending;
}

void TCPConnectionSchedule::refresh_wakeup_time()
{
    wakeup_tp.reset();
    wakeup_tp.consider(disconnectedVerified.timeout());
    wakeup_tp.consider(feelers.timeout());
}

// auto TCPConnectionSchedule::get_context(const TCPConnectRequest& r, ConnectionState cs) -> std::optional<FoundContext>
// {
//     if (auto p { find(r.address()) }; p) {
//         if (cs == ConnectionState::CONNECTED_INITIALIZED)
//             assert(p->verificationState == VerificationState::VERIFIED);
//
//         return FoundContext {
//             *p,
//             ReconnectContext {
//                 .prevWait { r.sleptFor },
//                 .endpointState = p->verificationState,
//                 .connectionState = cs,
//                 .pinned = pinned.contains(r.address()) }
//         };
//     }
//     return {};
// };
}
