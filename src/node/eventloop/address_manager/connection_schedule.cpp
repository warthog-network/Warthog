#include "connection_schedule.hpp"
#include <cassert>
#include <functional>
#include <random>
namespace connection_schedule {
using namespace std::chrono;
using namespace std::chrono_literals;

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

void EndpointData::add_source(Source s)
{
    sources.insert(s);
}

std::optional<time_point> EndpointData::try_pop(time_point now, std::vector<ConnectRequest>& out)
{
    if (pending == true)
        return {};
    if (!timer.expired(now))
        return timer.timeout();

    pending = true;
    out.push_back(ConnectRequest::outbound(address, timer.sleep_duration()));
    return {};
}

std::optional<time_point> EndpointData::timeout() const
{
    if (pending == true)
        return {};
    return timer.timeout();
}

void EndpointData::connection_established()
{
    pending = false;
    connected += 1;
    connectionLog.log_success();
}

time_point EndpointData::outbound_connected_ended(const ReconnectContext& c)
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

time_point EndpointData::update_timer(const ReconnectContext& c)
{
    const bool verified { c.endpointState == EndpointState::VERIFIED };
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

EndpointData* EndpointVector::move_entry(const EndpointAddress& key, EndpointVector& to)
{
    EndpointData* elem = nullptr;
    std::erase_if(data, [&key, &to, &elem](EndpointData& d) {
        if (d.address == key) {
            to.data.push_back(std::move(d));
            elem = &to.data.back();
            return true;
        }
        return false;
    });
    return elem;
}

std::pair<EndpointData&, bool> EndpointVector::emplace(const EndpointAddressItem& i)
{
    auto p { find(i.address) };
    if (p)
        return { *p, false };
    EndpointData& e { insert(i) };
    if (auto t { e.timeout() }; t)
        update_wakeup_time(*t);
    return { e, true };
}

void EndpointVector::pop_requests(time_point now, std::vector<ConnectRequest>& out)
{
    if (!wakeup_tp || wakeup_tp > now)
        return;
    wakeup_tp.reset();
    for (auto& e : data)
        update_wakeup_time(e.try_pop(now, out));
}

std::vector<EndpointAddress> EndpointVector::sample(size_t N) const{

    // sample from cache
    std::vector<EndpointAddress> out;
    std::sample(data.begin(), data.end(), std::back_inserter(out),
        N, std::mt19937 { std::random_device {}() });
    return out;

};

EndpointData& EndpointVector::insert(const EndpointAddressItem& i)
{
    return data.emplace_back(i);
}

void EndpointVector::update_wakeup_time(const std::optional<time_point>& tp)
{
    if (wakeup_tp < tp)
        wakeup_tp = tp;
}

EndpointData* EndpointVector::find(const EndpointAddress& address) const
{
    auto iter { std::find_if(data.begin(), data.end(), [&](auto& elem) { return elem.address == address; }) };
    if (iter == data.end())
        return nullptr;
    return &*iter;
}

auto ConnectionSchedule::find(const EndpointAddress& a) const -> std::optional<Found>
{
    using enum EndpointState;
    auto p = verified.find(a);
    if (p)
        return Found { *p, VERIFIED };
    if (p = unverifiedNew.find(a); p)
        return Found { *p, UNVERIFIED_NEW };
    if (p = unverifiedFailed.find(a); p)
        return Found { *p, UNVERIFIED_FAILED };
    return {};
}

std::optional<ConnectRequest> ConnectionSchedule::insert(EndpointAddressItem item)
{
    auto o { find(item.address) };
    if (o.has_value()) {
        // only track sources of addresses that are not verified
        if (o->state != EndpointState::VERIFIED)
            o->item.add_source(item.source);
        return {};
    } else {
        unverifiedNew.emplace(item);
        if (auto t { unverifiedNew.timeout() }; t)
            update_wakeup_time(*t);
        return ConnectRequest::outbound(item.address, 0s);
    }
}

void ConnectionSchedule::connection_established(const peerserver::ConnectionData& c)
{ // OK
    if (c.inbound())
        return;
    const EndpointAddress& ea { c.peer() };
    auto p { unverifiedNew.move_entry(ea, verified) };
    if (!p)
        p = unverifiedFailed.move_entry(ea, verified);
    if (!p)
        p = verified.find(ea);
    if (!p)
        return;
    p->connection_established();
}

void ConnectionSchedule::outbound_closed(const peerserver::ConnectionData& c)
{
    using enum ConnectionState;
    auto state { c.successfulConnection ? INITIALIZED : UNINITIALIZED };
    outbound_connection_ended(c.connectRequest, state);

    // TODO: make sure prune does not discard pending entries

    // reconnect?
    // * reconnect immediately if pinned
    // * outbound connect later if evil disconnect reason
    // * outbound connect immediately if different reason
    // * outbound don't connect if disconnected on purpose due to too many connections
}
void ConnectionSchedule::outbound_failed(const ConnectionData& c)
{
    outbound_connection_ended(c.connectRequest, ConnectionState::NOT_CONNECTED);
}

void ConnectionSchedule::outbound_connection_ended(const ConnectRequest& r, ConnectionState state)
{
    if (auto o { get_context(r, state) })
        update_wakeup_time(o->item.outbound_connected_ended(o->context));
}

void ConnectionSchedule::update_wakeup_time(const std::optional<time_point>& tp)
{
    if (tp && (!wakeup_tp || *tp < wakeup_tp))
        wakeup_tp = tp;
}

std::vector<ConnectRequest> ConnectionSchedule::pop_requests()
{
    auto now { steady_clock::now() };
    if (!wakeup_tp || wakeup_tp > now)
        return {};

    // pop requests
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
    update_wakeup_time(verified.timeout());
    update_wakeup_time(unverifiedNew.timeout());
    update_wakeup_time(unverifiedFailed.timeout());
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
}
