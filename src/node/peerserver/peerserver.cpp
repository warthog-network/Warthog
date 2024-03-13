#include "peerserver.hpp"
#include "asyncio/conman.hpp"
#include "asyncio/connection.hpp"
#include "config/config.hpp"
#include "connection_data.hpp"
#include "db/peer_db.hpp"
#include "general/now.hpp"
#include "global/globals.hpp"
namespace peerserver {
using namespace std::chrono;
using namespace std::chrono_literals;

size_t ConnectionLog::recent_failures() const
{
    size_t z(std::countr_zero(bits >> 5));
    size_t a { active_bits() };
    return std::min(z, a);
}

size_t ConnectionLog::log_failure()
{
    uint32_t active { active_bits() + 1 };
    if ((active >> 5) > 0) {
        active = 0x0000001Fu;
    }
    const auto logbits { bits >> 5 };
    bits = (logbits << 6) | active;
    uint32_t z(std::countr_zero(logbits) + 1);
    return std::min(z, active);
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
    connectionLog.log_success();
    verified = true;
    connected += 1;
}

void EndpointData::connection_closed()
{
    assert(!pending);
    assert(connected > 0);
    connected -= 1;
}

time_point EndpointData::failed_connection_closed(const ReconnectContext& c)
{
    assert(!pending);
    assert(connected > 0);
    pending = false;
    connected -= 1;
    return update_timer_failed(c);
};

time_point EndpointData::outbound_connect_failed(const ReconnectContext& c)
{
    pending = false;
    return update_timer_failed(c);
}

time_point EndpointData::update_timer_failed(const ReconnectContext& c)
{
    auto repeatedFailures { connectionLog.log_failure() };
    auto wait = std::invoke([&]() -> duration {
        if (verified && repeatedFailures == 1)
            return 0s; // immediately retry
        auto d { verified ? c.prevWait : 20s };
        if (d < 1s)
            return 1s;
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

std::optional<EndpointAddress> ConnectionSchedule::insert(EndpointAddressItem item)
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
        return item.address;
    }
}

void ConnectionSchedule::connection_established(const ConnectionData& c)
{
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

void ConnectionSchedule::outbound_closed(const ConnectionData& c)
{
    if (c.successfulConnection)
        successful_outbound_closed(c.connectRequest);
    else
        failed_outbound_closed(c.connectRequest);

    // reconnect?
    // * reconnect immediately if pinned
    // * outbound connect later if evil disconnect reason
    // * outbound connect immediately if different reason
    // * outbound don't connect if disconnected on purpose due to too many connections
}

void ConnectionSchedule::successful_outbound_closed(const ConnectRequest& r)
{
    if (auto p { verified.find(r.address) })
        update_wakeup_time(p->verified_outbound_closed(reconnect_context(r)));
}
void ConnectionSchedule::failed_outbound_closed(const ConnectRequest& r)
{
    if (auto p { find(r.address) })
        update_wakeup_time(p->failed_connection_closed(reconnect_context(r)));
}

void ConnectionSchedule::outbound_connect_failed(const ConnectRequest& r)
{
    if (auto c { get_reconnect_context(r) })
        update_wakeup_time(p->outbound_connect_failed(*c));
}

void ConnectionSchedule::update_wakeup_time(const std::optional<time_point>& tp)
{
    if (wakeup_tp < tp)
        wakeup_tp = tp;
}

std::vector<peerserver::ConnectRequest> ConnectionSchedule::pop_requests()
{
    auto now { steady_clock::now() };
    if (!wakeup_tp || wakeup_tp > now)
        return {};

    // pop requests
    std::vector<peerserver::ConnectRequest> res;
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

std::optional<ReconnectContext> ConnectionSchedule::get_reconnect_context(const ConnectRequest& r)
{
    if (auto p { find(r.address) }; p) {
        return ReconnectContext{
            .prevWait { r.sleptFor },
            .state = p->state ,
            .pinned = pinned.contains(r.address)
        };
    }
    return {};
};
}

namespace {
uint32_t bantime(int32_t /*offense*/)
{
    return 20 * 60; // 20 minutes;
}
} // namespace

PeerServer::PeerServer(PeerDB& db, const ConfigParams& config)
    : db(db)
    , enableBan(config.peers.enableBan)
{
    worker = std::thread(&PeerServer::work, this);
}
void PeerServer::register_close(IPv4 address, uint32_t now,
    int32_t offense, int64_t rowid)
{
    if (errors::is_malicious(offense)) {
        uint32_t banuntil = now + bantime(offense);
        db.set_ban(address, banuntil, offense);
        db.insert_offense(address, offense);
        bancache.set(address, banuntil);
    }
    if (rowid >= 0)
        db.insert_disconnect(rowid, now, offense);
}

void PeerServer::work()
{
    while (true) {
        decltype(events) tmpq;
        {
            std::unique_lock<std::mutex> l(mutex);
            if (shutdown)
                return;
            while (!hasWork) {
                cv.wait(l);
            }
            hasWork = false;
            std::swap(tmpq, events);
        }
        {
            auto t = db.transaction();
            now = now_timestamp();
            while (!tmpq.empty()) {
                std::visit([&](auto& e) {
                    handle_event(std::move(e));
                },
                    tmpq.front());
                tmpq.pop();
            }
            t.commit();
            process_timer();
        }
    }
}

void PeerServer::process_timer()
{
    for (auto& r : connectionSchedule.pop_requests())
        start_request(r);
}

void PeerServer::start_request(const peerserver::ConnectRequest& r)
{
    global().conman->connect(r);
}

void PeerServer::handle_event(SuccessfulOutbound&& e)
{
    e.c->successfulConnection = true;
    connectionSchedule.connection_established(*e.c);
}
void PeerServer::handle_event(VerifyPeer&&)
{
    // TODO
}
void PeerServer::handle_event(OnClose&& o)
{
    auto ip { o.con->peer().ipv4 };
    register_close(ip, now, o.offense, o.con->logrow);
};

void PeerServer::handle_event(Unban&& ub)
{
    bancache.clear();
    spdlog::info("Reset bans");
    db.reset_bans();
    ub.cb({});
};

void PeerServer::handle_event(GetOffenses&& go)
{
    go.cb(db.get_offenses(go.page));
};

void PeerServer::handle_event(Authenticate&& nc)
{
    // return EMAXCONNECTIONS; TODO: handle max connections
    auto& con = *nc.c;
    bool allowed = true;
    const IPv4& ip = con.peer().ipv4;
    uint32_t banuntil;
    int32_t offense;
    if (bancache.get(ip, banuntil) || db.get_peer(ip.data, banuntil, offense)) { // found entry
        if (enableBan == true && banuntil > now) {
            allowed = false;
            db.insert_refuse(ip, now);
        }
    } else {
        db.insert_peer(ip);
    };
    if (allowed) {
        con.logrow = db.insert_connect(ip.data, now);
        con.start_read();
    } else {
        con.close(EREFUSED);
    }
}

void PeerServer::handle_event(banned_callback_t&& cb)
{
    auto banned = db.get_banned_peers();
    cb(banned);
}
void PeerServer::handle_event(RegisterPeer&& e)
{
    db.peer_insert(e.a);
}
void PeerServer::handle_event(SeenPeer&& e)
{
    db.peer_seen(e.a, now);
}
void PeerServer::handle_event(GetRecentPeers&& e)
{
    e.cb(db.recent_peers(e.maxEntries));
}
void PeerServer::handle_event(Inspect&& e)
{
    e.cb(*this);
}

void PeerServer::handle_event(FailedConnect&& fc)
{
    spdlog::warn("Cannot connect to {}: ", fc.connectRequest.address.to_string(), Error(fc.reason).err_name());
    connectionSchedule.outbound_connect_failed(fc.connectRequest);
    // TODO
};
