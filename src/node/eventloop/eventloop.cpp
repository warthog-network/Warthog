#include "eventloop.hpp"
#include "address_manager/address_manager_impl.hpp"
#include "api/realtime.hpp"
#include "api/types/all.hpp"
#include "block/chain/header_chain.hpp"
#include "block/header/batch.hpp"
#include "block/header/view.hpp"
#include "chainserver/server.hpp"
#include "communication/buffers/sndbuffer.hpp"
#include "communication/messages_impl.hpp"
#include "global/globals.hpp"
#include "mempool/order_key.hpp"
#include "peerserver/peerserver.hpp"
#include "spdlog/spdlog.h"
#include "transport/webrtc/connection.hxx"
#include "transport/webrtc/sdp_util.hpp"
#include "types/conref_impl.hpp"
#include "types/peer_requests.hpp"
#include <algorithm>
#include <future>
#include <iostream>
#include <random>
#include <sstream>

template <typename... Args>
inline void log_communication(spdlog::format_string_t<Args...> fmt, Args&&... args)
{
    if (config().logCommunication)
        spdlog::info(fmt, std::forward<Args>(args)...);
}

using namespace std::chrono_literals;
Eventloop::Eventloop(Token, PeerServer& ps, ChainServer& cs, const ConfigParams& config)
    : stateServer(cs)
    , chains(cs.get_chainstate())
    , mempool(false)
#ifndef DISABLE_LIBUV
    , connections({ ps, config.peers.connect })
#else
    , connections({ ps })
#endif
    , headerDownload(chains, consensus().total_work())
    , blockDownload(*this)
{
    auto& ss = consensus().get_signed_snapshot();
    spdlog::info("Chain info: length {}, work {}, ", consensus().headers().length().value(), consensus().total_work().getdouble());
    if (ss.has_value()) {
        bool valid { ss->compatible(consensus().headers()) };
        spdlog::info("Chain snapshot is {}: priority {}, height {}", (valid ? "valid" : "invalid"), ss->priority.importance, ss->height().value());
    } else {
        spdlog::info("Chain snapshot not present");
    }
}
std::shared_ptr<Eventloop> Eventloop::create(PeerServer& ps, ChainServer& ss, const ConfigParams& config)
{
    return std::make_shared<Eventloop>(Token {}, ps, ss, config);
}

Eventloop::~Eventloop()
{
    wait_for_shutdown();
}
void Eventloop::start()
{
    assert(!worker.joinable());
    worker = std::thread(&Eventloop::loop, this);
}

bool Eventloop::defer(Event e)
{
    std::unique_lock<std::mutex> l(mutex);
    if (closeReason)
        return false;
    haswork = true;
    events.push(std::move(e));
    cv.notify_one();
    return true;
}

bool Eventloop::async_process(std::shared_ptr<ConnectionBase> c)
{
    return defer(OnProcessConnection { std::move(c) });
}

bool Eventloop::async_register(ConnectionBase::ConnectionVariant con)
{
    return defer(RegisterConnection(std::move(con)));
}

void Eventloop::shutdown(int32_t reason)
{
    std::unique_lock<std::mutex> l(mutex);
    haswork = true;
    closeReason = reason;
    cv.notify_one();
}
void Eventloop::wait_for_shutdown()
{
    if (worker.joinable())
        worker.join();
}

void Eventloop::erase(std::shared_ptr<ConnectionBase> c)
{
    defer(Erase { std::move(c) });
}

void Eventloop::async_state_update(StateUpdate&& s)
{
    defer(std::move(s));
}

void Eventloop::async_mempool_update(mempool::Log&& s)
{
    defer(std::move(s));
}

void Eventloop::start_timer(StartTimer st)
{
    defer(std::move(st));
}

void Eventloop::cancel_timer(const Timer::key_t& k)
{
    defer(CancelTimer { k });
}

void Eventloop::on_failed_connect(const ConnectRequest& r, Error reason)
{
    defer(FailedConnect { r, reason });
}

void Eventloop::api_get_peers(PeersCb&& cb)
{
    defer(std::move(cb));
}

void Eventloop::api_get_synced(SyncedCb&& cb)
{
    defer(std::move(cb));
}

void Eventloop::api_inspect(InspectorCb&& cb)
{
    defer(std::move(cb));
}
void Eventloop::api_get_hashrate(HashrateCb&& cb, size_t n)
{
    defer(GetHashrate { std::move(cb), n });
}

void Eventloop::api_get_hashrate_chart(NonzeroHeight from, NonzeroHeight to, size_t window, HashrateChartCb&& cb)
{
    defer(GetHashrateChart { std::move(cb), from, to, window });
}

void Eventloop::async_forward_blockrep(uint64_t conId, std::vector<BodyContainer>&& blocks)
{
    defer(OnForwardBlockrep { conId, std::move(blocks) });
}

void Eventloop::notify_closed_rtc(std::shared_ptr<RTCConnection> rtc)
{
    defer(RTCClosed { std::move(rtc) });
}

bool Eventloop::has_work()
{
    auto now = std::chrono::steady_clock::now();
    return haswork || (now > timer.next());
}

void Eventloop::loop()
{
    RTCConnection::fetch_id([w = weak_from_this()](IdentityIps&& ips) {
        if (auto p { w.lock() })
            p->defer(std::move(ips));
    },
        true);

    connections.start();
    while (true) {
        {
            std::unique_lock<std::mutex> ul(mutex);
            while (!has_work()) {
                auto until = timer.next();
                using namespace std::chrono;
                auto count = duration_cast<seconds>(until.time_since_epoch()).count();
                spdlog::debug("Eventloop wait until {} ms", count);
                cv.wait_until(ul, until);
            }
            haswork = false;
        }
        work();
        if (check_shutdown()) {
            return;
        }
    }
}

void Eventloop::work()
{
    decltype(events) tmp;
    std::vector<Timer::Event> expired;
    {
        std::unique_lock<std::mutex> l(mutex);
        std::swap(tmp, events);
        expired = timer.pop_expired();
    }
    // process expired
    for (auto& data : expired) {
        std::visit([&](auto&& e) {
            handle_timeout(std::move(e));
        },
            data);
    }
    while (!tmp.empty()) {
        std::visit([&](auto&& e) {
            handle_event(std::move(e));
        },
            tmp.front());
        tmp.pop();
    }
    connections.garbage_collect();
    update_sync_state();
    set_scheduled_connect_timer();
}

bool Eventloop::check_shutdown()
{

    {
        std::unique_lock<std::mutex> l(mutex);
        if (closeReason == 0)
            return false;
    }

    spdlog::debug("Shutdown connectionManager.size() {}", connections.size());
    for (auto cr : connections.all()) {
        if (cr->erased())
            continue;
        erase_internal(cr);
    }
    return true;
}

void Eventloop::handle_event(Erase&& m)
{
    bool erased { m.c->eventloop_erased };
    bool registered { m.c->eventloop_registered };
    if ((!erased) && registered)
        erase_internal(m.c->dataiter);
}

void Eventloop::handle_event(RegisterConnection&& m)
{
    auto r { try_register(std::move(m)) };

    if (r.has_value()) {
        send_init(r.value());
    } else {
        auto c { m.convar.base() };
        c->close(r.error());
        c->eventloop_erased = true;
    }
}

void Eventloop::handle_event(OnProcessConnection&& m)
{
    process_connection(std::move(m.c));
}

void Eventloop::handle_event(StateUpdate&& e)
{
    // mempool
    mempool.apply_log(std::move(e.mempoolUpdate));

    // header chain
    std::visit([&](auto&& action) {
        update_chain(std::move(action));
    },
        std::move(e.chainstateUpdate));
}

void Eventloop::update_chain(Append&& m)
{
    const auto msg = chains.update_consensus(std::move(m));
    log_chain_length();
    for (auto c : connections.all()) {
        try {
            if (c.initialized())
                c->chain.on_consensus_append(chains);
        } catch (ChainError e) {
            close(c, e);
        }
        c.send(msg);
    }
    //  broadcast new snapshot
    for (auto c : connections.initialized())
        consider_send_snapshot(c);

    coordinate_sync();
    do_requests();
}

void Eventloop::update_chain(Fork&& fork)
{
    const auto msg { chains.update_consensus(std::move(fork)) };
    log_chain_length();
    for (auto c : connections.all()) {
        try {
            if (c.initialized())
                c->chain.on_consensus_fork(msg.forkHeight(), chains);
            c.send(msg);
        } catch (ChainError e) {
            close(c, e);
        }
    }

    coordinate_sync();
    do_requests();
}

void Eventloop::update_chain(RollbackData&& rd)
{
    // update consensus
    const auto msg { chains.update_consensus(rd) };
    if (msg) {
        log_chain_length();
        for (auto c : connections.all()) {
            if (c.initialized())
                c->chain.on_consensus_shrink(chains);
            c.send(*msg);
        }
    }
    headerDownload.on_signed_snapshot_update();

    // update stage
    if (!rd.signedSnapshot.compatible(chains.stage_headers())) {
        blockDownload.reset();
    }

    //  broadcast new snapshot
    for (auto c : connections.initialized())
        consider_send_snapshot(c);

    coordinate_sync();
    syncdebug_log().info("init blockdownload update_chain");
    initialize_block_download();
    do_requests();
}

void Eventloop::coordinate_sync()
{
    auto cons { chains.consensus_state().headers().total_work() };
    auto blk { blockDownload.get_reachable_totalwork() };
    auto max { std::max(cons, blk) };
    headerDownload.set_min_worksum(max);
    blockDownload.set_min_worksum(cons);
}

void Eventloop::initialize_block_download()
{
    if (auto d { headerDownload.pop_data() }; d) {
        auto offenders = blockDownload.init(std::move(*d));
        for (ChainOffender& o : offenders) {
            close(o);
        };
        process_blockdownload_stage();
    }
}

ForkHeight Eventloop::set_stage_headers(Headerchain&& hc)
{
    spdlog::info("Syncing... (height {} of {})", chains.consensus_length().value(), hc.length().value());
    auto forkHeight { chains.update_stage(std::move(hc)) };
    return forkHeight;
}

void Eventloop::log_chain_length()
{
    auto synced { chains.consensus_length().value() };
    auto total { chains.stage_headers().length().value() };
    if (synced < total)
        spdlog::info("Syncing... (height {} of {})", synced, total);
    else if (synced == total)
        spdlog::info("Synced. (height {}).", synced);
}

void Eventloop::handle_event(PeersCb&& cb)
{
    std::vector<API::Peerinfo> out;
    for (auto cr : connections.initialized()) {
        out.push_back(API::Peerinfo {
            .endpoint { cr.peer() },
            .initialized = cr.initialized(),
            .chainstate = cr.chain(),
            .theirSnapshotPriority = cr->theirSnapshotPriority,
            .acknowledgedSnapshotPriority = cr->acknowledgedSnapshotPriority,
            .since = cr->c->created_at_timestmap(),
        });
    }
    cb(out);
}

void Eventloop::handle_event(SyncedCb&& cb)
{
    cb(!blockDownload.is_active());
}

void Eventloop::handle_event(SignedSnapshotCb&& cb)
{
    if (signed_snapshot()) {
        cb(*signed_snapshot());
    } else {
        cb(tl::make_unexpected(ENOTFOUND));
    }
}

void Eventloop::handle_event(stage_operation::Result&& r)
{
    auto offenders { blockDownload.on_stage_result(std::move(r)) };
    for (auto& o : offenders)
        close(o);
    process_blockdownload_stage();
    do_requests();
}

void Eventloop::handle_event(OnForwardBlockrep&& m)
{
    if (auto cr { connections.find(m.conId) }; cr) {
        BlockrepMsg msg((*cr)->lastNonce, std::move(m.blocks));
        cr->send(msg);
    }
}

void Eventloop::handle_event(InspectorCb&& cb)
{
    cb(*this);
}

void Eventloop::handle_event(GetHashrate&& e)
{
    e.cb(API::HashrateInfo {
        .nBlocks = e.n,
        .estimate = consensus().headers().hashrate(e.n) });
}

void Eventloop::handle_event(GetHashrateChart&& e)
{
    e.cb(consensus().headers().hashrate_chart(e.from, e.to, e.window));
}
void Eventloop::handle_event(FailedConnect&& e)
{
    spdlog::warn("Cannot connect to {}: ", e.connectRequest.address.to_string(), Error(e.reason).err_name());
    connections.outbound_failed(e.connectRequest);
    // TODO
}

void Eventloop::handle_event(mempool::Log&& log)
{
    mempool.apply_log(log);

    // build vector of mempool entries
    std::vector<mempool::Entry> entries;
    for (auto& action : log) {
        if (std::holds_alternative<mempool::Put>(action)) {
            entries.push_back(std::get<mempool::Put>(action).entry);
        }
    }
    std::sort(entries.begin(), entries.end(),
        [](const mempool::Entry& e1, const mempool::Entry& e2) {
            if (e1.transaction_height() == e2.transaction_height())
                return e1.transaction_id() < e2.transaction_id();
            return e1.transaction_height() < e2.transaction_height();
        });

    // construct subscription bounds per connection
    auto eiter = entries.begin();
    std::vector<std::pair<decltype(entries)::iterator, Conref>> bounds;
    auto miter = mempoolSubscriptions.cbegin();
    if (mempoolSubscriptions.size() > 0) {
        while (eiter != entries.end()) {
            while (!(eiter->transaction_height() < miter->first.transactionHeight)) {
                bounds.push_back({ eiter, miter->second });
                ++miter;
                if (miter == mempoolSubscriptions.cend())
                    goto finished;
            }
            ++eiter;
        }
        while (miter != mempoolSubscriptions.end()) {
            bounds.push_back({ entries.end(), miter->second });
            ++miter;
        }
    finished:

        // send subscription individually
        for (auto& [end, cr] : bounds) {
            cr.send(TxnotifyMsg::direct_send(entries.begin(), end));
        }
    }
}

void Eventloop::handle_event(StartTimer&& st)
{
    auto now = std::chrono::steady_clock::now();
    if (st.wakeup < now) {
        st.on_expire();
        return;
    }
    auto iter = timer.insert(st.wakeup, Timer::CallFunction(std::move(st.on_expire)));
    st.on_timerstart({ iter->first });
}

void Eventloop::handle_event(CancelTimer&& ct)
{
    timer.cancel(ct.timer);
}

void Eventloop::handle_event(RTCClosed&& ct)
{
    if (auto conId { ct.con->verification_con_id() }; conId != 0) { // conId id verified in this RTC connection
        if (auto con { connections.find(conId) }) {
            con->rtc().our.pendingVerification.done();
            rtc.verificationSchedule.add(*con);
        }
    }
    rtc.connections.erase(ct.con);
    try_verify_rtc_identities();
}

void Eventloop::handle_event(IdentityIps&& ips)
{
    spdlog::info("Webrtc identity IPv4: {}", ips.get_ip4() ? ips.get_ip4().value().to_string() : "N/A");
    spdlog::info("Webrtc identity IPv6: {}", ips.get_ip6() ? ips.get_ip6().value().to_string() : "N/A");

    assert(rtc.ips.has_value() == false);
    for (auto cr : connections.initialized()) {
        if (cr.version().v2()) {
            spdlog::info("Sending own identity");
            cr.send(RTCIdentity(ips));
        }
    }
    rtc.ips = std::move(ips);

    send_schedule_signaling_lists();
    for (auto c : connections.initialized()) {
        rtc.verificationSchedule.add(c);
    }
    try_verify_rtc_identities();
}

void Eventloop::handle_event(GeneratedVerificationSdpAnswer&& m)
{
    auto l { m.con.lock() };
    if (!l)
        return;
    auto& con { *l };
    auto orig { connections.find(m.originConId) };
    if (!orig.has_value()) {
        con.close(ERTCNOSIGNAL2);
        return;
    }
    auto& originCon { orig.value() };

    // filter
    auto filtered { sdp_filter::only_udp_ip(m.ownIp, m.sdp) };
    if (!filtered) {
        // We cannot offer what we promised
        close(originCon, ERTCOWNIP);
        return;
    }
    auto& sdp = *filtered;

    spdlog::info("send RTCVerificationAnswer, ip: {}", m.ownIp.to_string());
    originCon.send(RTCVerificationAnswer { sdp });
}
void Eventloop::handle_event(GeneratedSdpAnswer&& m)
{
    auto l { m.con.lock() };
    if (!l)
        return;
    auto& con { *l };
    auto sig { connections.find(m.signalingServerId) };
    if (!sig.has_value()) {
        con.close(ERTCNOSIGNAL2);
        return;
    }
    auto& signalingServer { sig.value() };

    // filter
    auto filtered { sdp_filter::only_udp_ip(m.ownIp, m.sdp) };
    if (!filtered) {
        // We cannot offer what we promised, TODO: think about other ways to handle this
        close(signalingServer, ERTCOWNIP);
        return;
    }
    auto& sdp = *filtered;

    signalingServer.send(RTCRequestForwardAnswer { sdp, m.key });
}

void Eventloop::handle_event(GeneratedVerificationSdpOffer&& m)
{
    auto l { m.con.lock() };
    if (!l)
        return;
    auto& rtcCon { *l };

    auto o { connections.find(m.peerId) };
    if (!o.has_value()) {
        rtcCon.close(ERTCNOPEER);
        return;
    }
    auto& c { o.value() };
    const auto& verifyIp { rtcCon.native_peer_addr().ip() };

    auto ips { IdentityIps::from_sdp(m.sdp) };

    std::optional<IP> selected { ips.get_ip_with_type(verifyIp.type()) };
    if (!selected.has_value()) {
        rtcCon.close(ERTCNOPEER);
        return;
    }

    spdlog::info("GeneratedVerificationSdpOffer: with IP {}", selected->to_string());
    auto filtered { sdp_filter::only_udp_ip(*selected, m.sdp) };
    assert(filtered.has_value());
    c.send(RTCVerificationOffer { verifyIp, filtered.value() });
}

void Eventloop::handle_event(GeneratedSdpOffer&& m)
{
    auto l { m.con.lock() };
    if (!l)
        return;
    auto& rtcCon { *l };

    auto sig { connections.find(m.signalingServerId) };
    if (!sig.has_value()) {
        rtcCon.close(ERTCNOSIGNAL);
        return;
    }
    auto& signalingServer { sig.value() };

    auto key { m.signalingListKey };
    // only send if the signaling server still supports the same list where
    // the key is taken from (no new RTCSignalingList message received since then)
    if (signalingServer.rtc().their.signalingList.covers(key)) {
        signalingServer.rtc().our.pendingOutgoing.insert(std::move(m.con));
        signalingServer.send(RTCRequestForwardOffer { key, std::move(m.sdp) });
    }
}

void Eventloop::erase_internal(Conref c)
{
    if (c->c->eventloop_erased)
        return;
    c->c->eventloop_erased = true;
    bool doRequests = false;
    c.job().unref_active_requests(activeRequests);
    if (c.ping().has_timerref(timer))
        timer.cancel(c.ping().timer());
    if (c.job().has_timerref(timer)) {
        timer.cancel(c.job().timer());
    }
    if (headerDownload.erase(c) && !closeReason) {
        realtime_api::on_disconnect(headerDownload.size(), c.id());
        // TODO: add log
        // spdlog::info("Connected to {} peers (closed connection to {}, reason: {})", headerDownload.size(), c->c->peer_endpoint().to_string(), Error(error).err_name());
    }
    if (blockDownload.erase(c))
        coordinate_sync();
    rtc.erase(c);
    connections.erase(c.iterator());
    if (doRequests) {
        do_requests();
    }
}

bool Eventloop::insert(Conref c, const InitMsg& data)
{
    bool doRequests = true;

    c->chain.initialize(data, chains);
    headerDownload.insert(c);
    blockDownload.insert(c);
    realtime_api::on_connect(headerDownload.size(), c);
    spdlog::info("Connected to {} peers (new peer {})", headerDownload.size(), c.peer().to_string());
    if (rtc.ips && c.version().v2()) {
        spdlog::info("Sending own identity");
        c.send(RTCIdentity(*rtc.ips));
    } else
        spdlog::info("NOT sending own identity");
    send_ping_await_pong(c);
    // LATER: return whether doRequests is necessary;
    return doRequests;
}

void Eventloop::close(Conref cr, Error reason)
{
    spdlog::info("close {}: {}", cr.peer().to_string(), reason.err_name());
    if (!cr->c->eventloop_registered)
        return;
    cr->c->close(reason);
    erase_internal(cr); // do not consider this connection anymore
}

void Eventloop::close_by_id(uint64_t conId, int32_t reason)
{
    if (auto cr { connections.find(conId) }; cr)
        close(*cr, reason);
    // LATER: report offense to peerserver
}

void Eventloop::close(const ChainOffender& o)
{
    assert(o);
    if (auto cr { connections.find(o.conId) }; cr) {
        close(*cr, o.e);
    } else {
        report(o);
    }
}
void Eventloop::close(Conref cr, ChainError e)
{
    assert(e);
    close(cr, e.e);
}

void Eventloop::process_connection(std::shared_ptr<ConnectionBase> c)
{
    if (c->eventloop_erased)
        return;
    assert(c->eventloop_registered);
    auto messages = c->pop_messages();
    Conref cr { c->dataiter };
    for (auto& msg : messages) {
        try {
            dispatch_message(cr, msg.parse());
        } catch (Error e) {
            close(cr, e.e);
            do_requests();
            break;
        }
        if (c->eventloop_erased) {
            return;
        }
    }
}

void Eventloop::send_ping_await_pong(Conref c)
{
    // do periodic tasks
    c.rtc().our.pendingForwards.prune([&](const rtc_state::PendingForwards::Entry& e) {
        if (auto con { connections.find(e.fromConId) }) {
            if (con->rtc().their.forwardRequests.is_accepted_key(e.fromKey)) {
                con->send(RTCForwardOfferDenied(e.fromKey, 1));
            }
        }
    });

    // send
    log_communication("{} Sending Ping", c.str());
    auto t = timer.insert(
        (config().localDebug ? 10min : 1min),
        Timer::CloseNoPong { c.id() });
    if (c.version().v1()) {
        PingMsg p(signed_snapshot() ? signed_snapshot()->priority : SignedSnapshot::Priority {});
        c.ping().await_pong(p, t);
        c.send(p);
    } else {
        PingV2Msg p(signed_snapshot() ? signed_snapshot()->priority : SignedSnapshot::Priority {},
            { .ndiscard = c.rtc().our.pendingOutgoing.schedule_discard() });
        c.ping().await_pong_v2({ .extraData { c.rtc().our.signalingList.offset_scheduled() },
                                   .msg = std::move(p) },
            t);
        c.send(p);
    }
}

void Eventloop::received_pong_sleep_ping(Conref c)
{
    auto t = timer.insert(10s, Timer::SendPing { c.id() });
    auto old_t = c.ping().sleep(t);
    cancel_timer(old_t);
}

void Eventloop::send_requests(Conref cr, const std::vector<Request>& requests)
{
    for (auto& r : requests) {
        std::visit([&](auto& req) {
            send_request(cr, req);
        },
            r);
    }
}

void Eventloop::do_requests()
{
start:
    auto offenders { headerDownload.do_requests(sender()) };
    if (offenders.size() > 0) {
        for (auto& o : offenders)
            close(o);
        goto start;
    }
    blockDownload.do_peer_requests(sender());
    headerDownload.do_probe_requests(sender());
    blockDownload.do_probe_requests(sender());
}

template <typename T>
void Eventloop::send_request(Conref c, const T& req)
{
    log_communication("{} send {}", c.str(), req.log_str());
    auto t = timer.insert(req.expiry_time, Timer::Expire { c.id() });
    c.job().assign(t, timer, req);
    if (req.isActiveRequest) {
        assert(activeRequests < maxRequests);
        activeRequests += 1;
    }
    c.send(req);
}

void Eventloop::send_init(Conref cr)
{
    cr.send(InitMsg::serialize_chainstate(consensus()));
}

template <typename T>
requires std::derived_from<T, Timer::WithConnecitonId>
void Eventloop::handle_timeout(T&& t)
{
    if (auto cr { connections.find(t.conId) }; cr.has_value()) {
        handle_connection_timeout(*cr, std::move(t));
    }
}
void Eventloop::handle_connection_timeout(Conref cr, Timer::CloseNoReply&&)
{
    cr.job().reset_expired(timer);
    close(cr, ETIMEOUT);
}
void Eventloop::handle_connection_timeout(Conref cr, Timer::CloseNoPong&&)
{
    cr.ping().reset_expired(timer);
    close(cr, ETIMEOUT);
}

void Eventloop::handle_timeout(Timer::ScheduledConnect&&)
{
    connections.start_scheduled_connections();
}

void Eventloop::handle_timeout(Timer::CallFunction&& cf)
{
    cf.callback();
}

void Eventloop::handle_timeout(Timer::SendIdentityIps&&)
{
    send_schedule_signaling_lists();
}

void Eventloop::handle_connection_timeout(Conref cr, Timer::SendPing&&)
{
    cr.ping().timer_expired(timer);
    return send_ping_await_pong(cr);
}

void Eventloop::handle_connection_timeout(Conref cr, Timer::Expire&&)
{
    cr.job().restart_expired(timer.insert(
                                 (config().localDebug ? 10min : 2min), Timer::CloseNoReply { cr.id() }),
        timer);
    assert(!cr.job().data_v.valueless_by_exception());
    std::visit(
        [&]<typename T>(T& v) {
            if constexpr (std::is_base_of_v<IsRequest, T>) {
                v.unref_active_requests(activeRequests);
                on_request_expired(cr, v);
            } else {
                assert(false);
            }
        },
        cr.job().data_v);
    assert(!cr.job().data_v.valueless_by_exception());
}

void Eventloop::on_request_expired(Conref cr, const Proberequest&)
{
    headerDownload.on_probe_request_expire(cr);
    blockDownload.on_probe_expire(cr);
    do_requests();
}

void Eventloop::on_request_expired(Conref cr, const Batchrequest& req)
{
    headerDownload.on_request_expire(cr, req);
    do_requests();
}

void Eventloop::on_request_expired(Conref cr, const Blockrequest&)
{
    blockDownload.on_blockreq_expire(cr);
    do_requests();
}

void Eventloop::dispatch_message(Conref cr, messages::Msg&& m)
{
    using namespace messages;

    // first message must be of type INIT (is_init() is only initially true)
    if (cr.job().awaiting_init()) {
        if (!std::holds_alternative<InitMsg>(m)) {
            throw Error(ENOINIT);
        }
    } else {
        if (std::holds_alternative<InitMsg>(m))
            throw Error(EINVINIT);
    }

    std::visit([&](auto&& e) {
        handle_msg(cr, std::move(e));
    },
        m);
}

void Eventloop::handle_msg(Conref cr, InitMsg&& m)
{
    log_communication("{} handle init: height {}, work {}", cr.str(), m.chainLength.value(), m.worksum.getdouble());
    cr.job().reset_notexpired<AwaitInit>(timer);
    if (insert(cr, m))
        do_requests();
}

void Eventloop::handle_msg(Conref cr, AppendMsg&& m)
{
    log_communication("{} handle append", cr.str());
    cr->chain.on_peer_append(m, chains);
    headerDownload.on_append(cr);
    blockDownload.on_append(cr);
    do_requests();
}

void Eventloop::handle_msg(Conref c, SignedPinRollbackMsg&& m)
{
    log_communication("{} handle rollback ", c.str());
    verify_rollback(c, m);
    c->chain.on_peer_shrink(m, chains);
    headerDownload.on_rollback(c);
    blockDownload.on_rollback(c);
    do_requests();
}

void Eventloop::handle_msg(Conref c, ForkMsg&& m)
{
    log_communication("{} handle fork", c.str());
    c->chain.on_peer_fork(m, chains);
    headerDownload.on_fork(c);
    blockDownload.on_fork(c);
    do_requests();
}

void Eventloop::handle_msg(Conref c, PingMsg&& m)
{
    log_communication("{} handle ping", c.str());
    c->ratelimit.ping();
#ifndef DISABLE_LIBUV // TODO: replace TCPSockaddr by something else for emscrpiten build (no TCP connections available in browsers)
    size_t nAddr { std::min(uint16_t(20), m.maxAddresses()) };
    auto addresses = connections.sample_verified_tcp(nAddr);
    PongMsg msg { m.nonce(), std::move(addresses), mempool.sample(m.maxTransactions()) };
    spdlog::debug("{} Sending {} addresses", c.str(), msg.addresses().size());
#else
    PongMsg msg(m.nonce(), {}, {});
#endif
    if (c->theirSnapshotPriority < m.sp())
        c->theirSnapshotPriority = m.sp();
    c.send(msg);
    consider_send_snapshot(c);
}

void Eventloop::handle_msg(Conref c, PingV2Msg&& m)
{

    log_communication("{} handle ping", c.str());
    c->ratelimit.ping();
#ifndef DISABLE_LIBUV // TODO: replace TCPSockaddr by something else for emscrpiten build (no TCP connections available in browsers)
    size_t nAddr { std::min(uint16_t(20), m.maxAddresses()) };
    auto addresses = connections.sample_verified_tcp(nAddr);
    PongV2Msg msg { m.nonce(), std::move(addresses), mempool.sample(m.maxTransactions()) };
#else
    PongV2Msg msg(m.nonce(), {}, {});
#endif
    spdlog::debug("{} Sending {} addresses", c.str(), msg.addresses().size());
    if (c->theirSnapshotPriority < m.sp())
        c->theirSnapshotPriority = m.sp();
    c.send(msg);
    consider_send_snapshot(c);
    c.rtc().their.forwardRequests.discard(m.discarded_forward_requests());
};

void Eventloop::handle_msg(Conref cr, PongMsg&& m)
{
    log_communication("{} handle pong", cr.str());
    auto& pingMsg = cr.ping().check(m);
    received_pong_sleep_ping(cr);
    spdlog::debug("{} Received {} addresses", cr.str(), m.addresses().size());
    // connections.verify(m.addresses,cr.);
    spdlog::debug("{} Got {} transaction Ids in pong message", cr.str(), m.txids().size());

    // update acknowledged priority
    if (cr->acknowledgedSnapshotPriority < pingMsg.sp()) {
        cr->acknowledgedSnapshotPriority = pingMsg.sp();
    }

    // request new txids
    auto txids = mempool.filter_new(m.txids());
    if (txids.size() > 0)
        cr.send(TxreqMsg(txids));
}

void Eventloop::handle_msg(Conref cr, PongV2Msg&& m)
{
    log_communication("{} handle pong", cr.str());
    auto& pingData = cr.ping().check(m);
    auto& pingMsg = pingData.msg;
    received_pong_sleep_ping(cr);
    spdlog::debug("{} Received {} addresses", cr.str(), m.addresses().size());
    // connections.verify(m.addresses,cr.);
    spdlog::debug("{} Got {} transaction Ids in pong message", cr.str(), m.txids().size());

    // update acknowledged priority
    if (cr->acknowledgedSnapshotPriority < pingMsg.sp()) {
        cr->acknowledgedSnapshotPriority = pingMsg.sp();
    }

    // request new txids
    auto txids = mempool.filter_new(m.txids());
    if (txids.size() > 0)
        cr.send(TxreqMsg(txids));

    // peer has seen the ping message and we can be sure it must have
    // acknowledged discarding, we replay.
    cr.rtc().our.pendingOutgoing.discard(pingMsg.discarded_forward_requests());
    cr.rtc().our.signalingList.discard_up_to(pingData.extraData.signalingListDiscardIndex);
}

void Eventloop::handle_msg(Conref cr, BatchreqMsg&& m)
{
    log_communication("{} handle batchreq [{},{}]", cr.str(), m.selector().startHeight.value(), (m.selector().startHeight + m.selector().length - 1).value());
    auto& s = m.selector();
    Batch batch = [&]() {
        if (s.descriptor == consensus().descriptor()) {
            return consensus().headers().get_headers(s.startHeight, s.end());
        } else {
            return stateServer.get_headers(s);
        }
    }();

    BatchrepMsg rep(m.nonce(), std::move(batch));
    cr.send(rep);
}

void Eventloop::handle_msg(Conref cr, BatchrepMsg&& m)
{
    log_communication("{} handle_batchrep", cr.str());
    // check nonce and get associated data
    auto req = cr.job().pop_req(m, timer, activeRequests);

    // save batch
    if (m.batch().size() < req.minReturn || m.batch().size() > req.max_return()) {
        close(ChainOffender(EBATCHSIZE, req.selector().startHeight, cr.id()));
        return;
    }
    auto offenders = headerDownload.on_response(cr, std::move(req), std::move(m.batch()));
    for (auto& o : offenders) {
        close(o);
    }

    syncdebug_log().info("init blockdownload batch_rep");
    initialize_block_download();

    // assign work
    do_requests();
}

void Eventloop::handle_msg(Conref cr, ProbereqMsg&& m)
{
    log_communication("{} handle_probereq d:{}, h:{}", cr.str(), m.descriptor().value(), m.height().value());
    ProberepMsg rep(m.nonce(), consensus().descriptor().value());
    auto h = consensus().headers().get_header(m.height());
    if (h)
        rep.current() = *h;
    if (m.descriptor() == consensus().descriptor()) {
        auto h = consensus().headers().get_header(m.height());
        if (h)
            rep.requested() = h;
    } else {
        auto h = stateServer.get_descriptor_header(m.descriptor(), m.height());
        if (h)
            rep.requested() = *h;
    }
    cr.send(rep);
}

void Eventloop::handle_msg(Conref cr, ProberepMsg&& rep)
{
    log_communication("{} handle_proberep", cr.str());
    auto req = cr.job().pop_req(rep, timer, activeRequests);
    if (!rep.requested().has_value() && !req.descripted->expired()) {
        throw ChainError { EEMPTY, req.height() };
    }
    cr->chain.on_proberep(req, rep, chains);
    headerDownload.on_proberep(cr, req, rep);
    blockDownload.on_probe_reply(cr, req, rep);
    do_requests();
}

void Eventloop::handle_msg(Conref cr, BlockreqMsg&& m)
{
    using namespace std::placeholders;
    BlockreqMsg req(m);
    log_communication("{} handle_blockreq [{},{}]", cr.str(), req.range().lower.value(), req.range().upper.value());
    cr->lastNonce = req.nonce();
    stateServer.async_get_blocks(req.range(), std::bind(&Eventloop::async_forward_blockrep, this, cr.id(), _1));
}

void Eventloop::handle_msg(Conref cr, BlockrepMsg&& m)
{
    log_communication("{} handle blockrep", cr.str());
    auto req = cr.job().pop_req(m, timer, activeRequests);

    try {
        blockDownload.on_blockreq_reply(cr, std::move(m), req);
        process_blockdownload_stage();
    } catch (Error e) {
        close(cr, e);
    }
    do_requests();
}

void Eventloop::handle_msg(Conref cr, TxnotifyMsg&& m)
{
    log_communication("{} handle Txnotify", cr.str());
    auto txids = mempool.filter_new(m.txids());
    if (txids.size() > 0)
        cr.send(TxreqMsg(txids));
    do_requests();
}

void Eventloop::handle_msg(Conref cr, TxreqMsg&& m)
{
    log_communication("{} handle TxreqMsg", cr.str());
    TxrepMsg::vector_t out;
    for (auto& e : m.txids()) {
        out.push_back(mempool[e]);
    }
    if (out.size() > 0)
        cr.send(TxrepMsg(m.nonce(), out));
}

void Eventloop::handle_msg(Conref cr, TxrepMsg&& m)
{
    log_communication("{} handle TxrepMsg", cr.str());
    std::vector<TransferTxExchangeMessage> txs;
    for (auto& o : m.txs()) {
        if (o)
            txs.push_back(*o);
    };
    stateServer.async_put_mempool(std::move(txs));
    do_requests();
}

void Eventloop::handle_msg(Conref cr, LeaderMsg&& msg)
{
    log_communication("{} handle LeaderMsg", cr.str());
    // ban if necessary
    if (msg.signedSnapshot().priority <= cr->acknowledgedSnapshotPriority) {
        close(cr, ELOWPRIORITY);
        return;
    }

    // update knowledge about sender
    cr->acknowledgedSnapshotPriority = msg.signedSnapshot().priority;
    if (cr->theirSnapshotPriority < msg.signedSnapshot().priority) {
        cr->theirSnapshotPriority = msg.signedSnapshot().priority;
    }

    stateServer.async_set_signed_checkpoint(msg.signedSnapshot());
}

void Eventloop::send_schedule_signaling_lists()
{
    // build map
    std::vector<std::pair<Conref, size_t>> quotasVec;
    std::vector<Conref> conrefs;
    for (auto c : connections.initialized()) {
        if (c.version().v1())
            continue;
        auto avail { c.rtc().their.quota.available() };
        quotasVec.push_back({ c, avail });
        conrefs.push_back(c);
    }
    spdlog::info("send_schedule_signaling_lists to {} peers", conrefs.size());

    // shuffle connections
    std::random_device rd;
    std::mt19937 g(rd());
    std::ranges::shuffle(conrefs, g);

    // for random boolean sampling
    std::uniform_int_distribution<> d(0, 1);

    // send and save
    for (auto& c : conrefs) {
        auto& identity { c.rtc().their.identity };
        auto v4 { identity.verified_ip4() };
        auto v6 { identity.verified_ip6() };

        std::vector<IP> ips;
        std::vector<std::pair<uint64_t, IP>> conIds;

        for (auto& [c, quota] : quotasVec) {
            if (quota == 0)
                continue;
            quota -= 1;
            auto use_ip = [&](IP ip) {
                ips.push_back(ip);
                conIds.push_back({ c.id(), ip });
            };
            if (v4) {
                if (v6) { // if we verified both, ipv6 and ipv6, then we sample
                    bool randBool = d(g) == 1;
                    if (randBool)
                        use_ip(*v4);
                    else
                        use_ip(*v6);
                } else {
                    use_ip(*v4);
                }
            } else if (v6) {
                use_ip(*v6);
            }
        }
        c.rtc().our.signalingList.set(conIds);
        c.send(RTCSignalingList(std::move(ips)));
    }
    timer.insert(1min, Timer::SendIdentityIps {});
}

void Eventloop::handle_msg(Conref c, RTCIdentity&& msg)
{
    spdlog::info("Received rtc identity");
    // TODO: restrict number of identity messages
    c.rtc().their.identity.set(msg.ips());

    rtc.verificationSchedule.add(c);
    try_verify_rtc_identities();
}

void Eventloop::handle_msg(Conref c, RTCQuota&& msg)
{
    c.rtc().their.quota.increase_allowed(msg.increase());
}

void Eventloop::handle_msg(Conref c, RTCSignalingList&& s)
{
    spdlog::warn("Received RTCSignalingList");
    const auto& ips { s.ips() };
    const auto offset {
        c.rtc().their.signalingList.set_new_list_size(ips.size())
    };
    for (size_t i = 0; i < ips.size(); ++i) {
        if (!c.rtc().our.pendingOutgoing.can_connect())
            break;

        auto& ip = s.ips()[i];
        if (connections.ip_count(ip) > 0)
            continue;

        uint64_t signalingListKey { offset + i };
        rtc.connections.insert(
            RTCConnection::connect_new(
                *this,
                [&, id = c.id(), signalingListKey](RTCConnection& con, std::string sdp) {
                    defer(GeneratedSdpOffer {
                        .con { con.weak_from_this() },
                        .signalingServerId = id,
                        .signalingListKey = signalingListKey,
                        .sdp { std::move(sdp) } });
                },
                ip));
    }
}

void Eventloop::handle_msg(Conref cr, RTCRequestForwardOffer&& r)
{
    auto dstId { cr.rtc().our.signalingList.get_con_id(r.signaling_list_key()) };
    auto key { cr.rtc().their.forwardRequests.create() };
    if (!dstId) {
        cr.send(RTCForwardOfferDenied { key, 0 });
        return;
    }
    auto ip { sdp_filter::load_ip(r.offer()) };
    if (!ip)
        throw Error(ERTCUNIQUEIP_RFO);
    if (!cr.rtc().their.identity.ip_is_verified(*ip))
        throw Error(ERTCUNVERIFIEDIP);

    auto dst_opt { connections.find(*dstId) };
    if (!dst_opt)
        return;
    auto& dstCon { *dst_opt };
    dstCon.rtc().our.pendingForwards.add(*ip, key, cr.id());
    assert(dstCon.rtc().their.quota.take_one());
    dstCon.send(RTCForwardedOffer { r.offer() });
}

void Eventloop::handle_msg(Conref cr, RTCForwardedOffer&& m)
{
    // check our quota assigned to that peeer
    auto key { cr.rtc().our.quota.take_one() };
    OneIpSdp oneIpSdp { std::move(m.offer()) };
    auto inType { oneIpSdp.ip().type() };
    auto ownIp { rtc.get_ip(inType) };
    if (!ownIp)
        throw Error(ERTCWRONGIP_FO);
    // TODO: authenticate also RTCConnections
    rtc.connections.insert(
        RTCConnection::accept_new(
            *this, [w = weak_from_this(), id = cr.id(), key, ownIp = *ownIp](RTCConnection& con, std::string sdp) {
                if (auto e { w.lock() }; e) {
                    e->defer(GeneratedSdpAnswer {
                        .ownIp { ownIp },
                        .con { con.weak_from_this() },
                        .signalingServerId = id,
                        .key = key,
                        .sdp { std::move(sdp) } });
                }
            },
            std::move(oneIpSdp)));
}

void Eventloop::handle_msg(Conref cr, RTCVerificationOffer&& m)
{
    OneIpSdp oneIpSdp { std::move(m.offer()) };
    // TODO: check m.ip() ip was indeed offered before by us as identity
    // TODO: rate limit this function
    rtc.connections.insert(
        RTCConnection::accept_new_verification(
            *this, [w = weak_from_this(), ip = m.ip(), id = cr.id()](RTCConnection& con, std::string sdp) {
                if (auto e { w.lock() }; e) {
                    // TODO: make to always reply to cr
                    e->defer(GeneratedVerificationSdpAnswer {
                        .ownIp { ip },
                        .con { con.weak_from_this() },
                        .originConId = id,
                        .sdp { std::move(sdp) } });
                }
            },
            std::move(oneIpSdp), cr.id()));
}

void Eventloop::handle_msg(Conref cr, RTCRequestForwardAnswer&& r)
{
    auto fwdInfo { cr.rtc().our.pendingForwards.get(r.key()) };
    auto ip { sdp_filter::load_ip(r.answer().data) };
    if (!ip)
        throw Error(ERTCUNIQUEIP_RFA);
    if (fwdInfo.ip != ip)
        throw Error(ERTCWRONGIP_RFA);

    if (auto o { connections.find(fwdInfo.fromConId) }; o.has_value()) {
        Conref& origin { *o };
        if (origin.rtc().their.forwardRequests.is_accepted_key(fwdInfo.fromKey)) {
            origin.send(RTCForwardedAnswer(fwdInfo.fromKey, std::move(r.answer())));
        }
    }
}

void Eventloop::handle_msg(Conref cr, RTCForwardOfferDenied&& m)
{
    auto wcon { cr.rtc().our.pendingOutgoing.get_rtc_con(m.key()) };
    if (auto pcon { wcon.lock() })
        pcon->close(ERTCFWDREJECT); // TODO count pending per node
}

void Eventloop::handle_msg(Conref cr, RTCForwardedAnswer&& a)
{
    auto w { cr.rtc().our.pendingOutgoing.get_rtc_con(a.key()) };
    auto c { w.lock() };
    if (!c)
        return;
    OneIpSdp ois(a.answer());

    if (auto error { c->set_sdp_answer(ois) })
        throw Error(*error);
}

void Eventloop::handle_msg(Conref cr, RTCVerificationAnswer&& m)
{
    spdlog::info("received RTCVerificationAnswer");
    // TODO:
    // - clear pending on main connection close
    // - callback on connection fail
    auto& pv { cr.rtc().our.pendingVerification };
    if (!pv.has_value())
        throw Error(ERTCUNEXP_VA);
    const auto& rtcCon { pv.value().con };
    OneIpSdp ois { m.answer() };
    if (auto e { rtcCon->set_sdp_answer(ois) })
        throw Error(*e);
}

void Eventloop::try_verify_rtc_identities()
{
    spdlog::info("try_verify_rtc_identities {}", rtc.connections.can_insert_feeler());
    if (rtc.verificationSchedule.empty() || rtc.ips->has_value() == false)
        return;
    while (rtc.connections.can_insert_feeler()) {
        auto p { rtc.ips->pattern() };
        auto o { rtc.verificationSchedule.pop(p) };
        bool b { o.has_value() };
        if (!b)
            return;
        auto& [c, ip] = *o;
        auto newCon { RTCConnection::connect_new_verification(
            *this,
            [this, peerId = c.id()](RTCConnection& con, std::string sdp) {
                defer(GeneratedVerificationSdpOffer {
                    .con { con.weak_from_this() },
                    .peerId = peerId,
                    .sdp { std::move(sdp) } });
            },
            ip, c.id()) };
        rtc.connections.insert(newCon);
        c.rtc().our.pendingVerification.start(std::move(newCon));
    }
}

void Eventloop::consider_send_snapshot(Conref c)
{
    // spdlog::info("
    if (signed_snapshot().has_value()) {
        auto theirPriority = c->theirSnapshotPriority;
        auto snapshotPriority = signed_snapshot()->priority;
        if (theirPriority < snapshotPriority) {
            c.send(LeaderMsg(*signed_snapshot()));
            c->theirSnapshotPriority = signed_snapshot()->priority;
        }
    }
}

void Eventloop::process_blockdownload_stage()
{
    auto r { blockDownload.pop_stage() };
    if (r)
        stateServer.async_stage_request(*r);
}

void Eventloop::async_stage_action(stage_operation::Result r)
{
    defer(std::move(r));
}

void Eventloop::cancel_timer(Timer::iterator& ref)
{
    timer.cancel(ref);
    ref = timer.end();
}

void Eventloop::verify_rollback(Conref cr, const SignedPinRollbackMsg& m)
{
    if (cr.chain().descripted()->chain_length() <= m.shrinkLength())
        throw Error(EBADROLLBACKLEN);
    auto& ss = m.signedSnapshot();
    if (cr.chain().stage_fork_range().lower() > ss.priority.height) {
        if (ss.compatible(chains.stage_headers()))
            throw Error(EBADROLLBACK);
    } else if (cr.chain().consensus_fork_range().lower() > ss.priority.height) {
        if (ss.compatible(chains.consensus_state().headers()))
            throw Error(EBADROLLBACK);
    }
}

tl::expected<Conref, int32_t> Eventloop::try_register(RegisterConnection&& m)
{
    auto c { m.convar.base() };
    c->eventloop_registered = true;

    if (m.convar.is_rtc()) {
        auto& c { m.convar.get_rtc() };
        auto& conId { m.convar.get_rtc()->verification_con_id() };
        if (conId != 0) { // conId id verified in this RTC connection
            if (auto o { connections.find(conId) }) {
                auto ip { c->native_peer_addr().ip() };
                auto& parent = *o;
                parent.rtc().their.identity.set_verified(ip);
                spdlog::info("verified RTC ip {} for {}", ip.to_string(), parent.peer().to_string());
            }
            conId = 0;
            return tl::make_unexpected(ERTCFEELER);
        }
    }

    return connections.insert({ .convar { m.convar },
        .headerDownload { headerDownload },
        .blockDownload { blockDownload },
        .timer { timer },
        .evict_cb {
            [this](Conref evictionCandidate) {
                close(evictionCandidate, EEVICTED);
            } } });
}
void Eventloop::update_sync_state()
{
    syncState.set_has_connections(!connections.initialized().empty());
    syncState.set_block_download(blockDownload.is_active());
    syncState.set_header_download(headerDownload.is_active());
    if (auto c { syncState.detect_change() }; c) {
        global().chainServer->async_set_synced(c.value());
    }
}

void Eventloop::set_scheduled_connect_timer()
{
    auto t { connections.pop_scheduled_connect_time() };
    if (!t)
        return;
    auto& tp { *t };

    if (wakeupTimer) {
        if ((*wakeupTimer)->first.wakeup_tp <= tp)
            return;
        timer.cancel(*wakeupTimer);
    }
    timer.insert(tp, Timer::ScheduledConnect {});
}
