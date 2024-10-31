#include "peerserver.hpp"
#include "config/config.hpp"
#include "connection_data.hpp"
#include "db/peer_db.hpp"
#include "general/now.hpp"
#include "global/globals.hpp"
#include "spdlog/spdlog.h"


PeerServer::PeerServer(PeerDB& db, const ConfigParams& config)
    : db(db)
    , enableBan(config.peers.enableBan)
{
}

void PeerServer::wait_for_shutdown()
{
    if (worker.joinable())
        worker.join();
}
void PeerServer::start()
{
    assert(!worker.joinable());
    worker = std::thread(&PeerServer::work, this);
}

void PeerServer::work()
{
    while (true) {
        decltype(events) tmpq;
        {
            std::unique_lock<std::mutex> l(mutex);
            if (_shutdown)
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
        }
    }
}

void PeerServer::handle_event(OnClose&& o)
{
    o.con->peer_addr().visit(
        [this, &o](auto&& socketAddr) -> void {
            on_close_internal(o, socketAddr);
        });
}

void PeerServer::handle_event(Unban&& ub)
{
    bancache.clear();
    spdlog::info("Reset bans");
    db.reset_bans();
    ub.cb({});
}

void PeerServer::handle_event(GetOffenses&& go)
{
    go.cb(db.get_offenses(go.page));
}

void PeerServer::handle_event(AuthenticateInbound&& nc)
{
    // return EMAXCONNECTIONS; TODO: handle max connections
    auto& con = *nc.c;
    auto& ip = nc.ip;
    if (!config().allowedInboundTransports.allowed(nc.ip, nc.transportType)) {
        db.insert_refuse(ip, now);
        con.close(EREFUSED);
        return;
    }
    auto bannedUntil = [this, &ip]() -> std::optional<Timestamp> {
        if (auto res { bancache.get_expiration(ip) }) {
            return Timestamp::from_time_point(*res);
        }
        if (auto res { db.get_peer(ip) }) {
            return res->banUntil;
        }
        return {};
    }();
    if (enableBan && bannedUntil && *bannedUntil > now) {
        db.insert_refuse(ip, now);
        con.close(EREFUSED);
    } else {
        db.insert_clear_ban(ip);
        con.logrow = db.insert_connect(ip, true, now);
        con.start_read();
    }
}

void PeerServer::handle_event(LogOutboundIPv4&& nc)
{
    auto& con = *nc.c;
    con.logrow = db.insert_connect(nc.ip, false, now);
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
    std::vector<std::pair<TCPPeeraddr, Timestamp>> res;

#ifndef DISABLE_LIBUV
    // TCP peers
    for (auto& [addr, lastseen] : db.recent_peers(e.maxEntries))
        res.push_back({ addr, lastseen });
#else
    // WS peers will be added manually from javascript side.
#endif
    e.cb(std::move(res));
}
void PeerServer::handle_event(Inspect&& e)
{
    e.cb(*this);
}

void PeerServer::on_close_internal(const OnClose&, const WebRTCPeeraddr& /*addr*/)
{
    // do nothing
}

#ifndef DISABLE_LIBUV
void PeerServer::on_close_internal(const OnClose& o, const Sockaddr& addr)
{
    auto ip { addr.ip };
    auto offense { o.offense };
    if (!ip.is_loopback() // don't ban localhost
        && offense.triggers_ban()) {
        uint32_t banuntil = now + offense.bantime();
        if (ip.is_v4()) {
            auto ip4{ip.get_v4()};
            db.set_ban(ip4, banuntil, offense);
            db.insert_offense(ip4, offense);
        }else{
            auto ip6{ip.get_v6()};
            db.set_ban(ip6.block48_view(), banuntil, offense);
            db.insert_offense(ip6, offense);
        }

        bancache.set(ip, banuntil);
    }
    if (o.con->logrow >= 0)
        db.insert_disconnect(o.con->logrow, now, offense);
}
#else
void PeerServer::on_close_internal(const OnClose& o, const WSUrladdr&)
{
    // do nothing, it was an outbound websocket connection from our browser
}
#endif
