#include "peerserver.hpp"
#include "config/config.hpp"
#include "connection_data.hpp"
#include "db/peer_db.hpp"
#include "general/now.hpp"
#include "global/globals.hpp"

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
void PeerServer::register_close(IPv4 address, uint32_t now,
    int32_t offense, int64_t rowid)
{

    if (!address.is_loopback()  // don't ban localhost
            && errors::leads_to_ban(offense)) {
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
            on_close(o, socketAddr);
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

void PeerServer::handle_event(Authenticate&& nc)
{
    // return EMAXCONNECTIONS; TODO: handle max connections
    auto& con = *nc.c;
    bool allowed = true;
    const IPv4& ip = con.peer_ipv4();
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
    std::vector<std::pair<TCPSockaddr, uint32_t>> res;

#ifndef DISABLE_LIBUV
    // TCP peers
    for (auto& [addr, lastseen] : db.recent_peers(e.maxEntries))
        res.push_back({ addr, lastseen });
#else
    // WS peers
    for (auto& [addr, lastseen] : db.recent_ws_peers(e.maxEntries))
        res.push_back({ addr, lastseen });
#endif
    e.cb(std::move(res));
}
void PeerServer::handle_event(Inspect&& e)
{
    e.cb(*this);
}

void PeerServer::on_close(const OnClose& o, const TCPSockaddr& addr)
{
    auto ip { addr.ip };
    register_close(ip, now, o.offense, o.con->logrow);
}
void PeerServer::on_close(const OnClose&, const WebRTCSockaddr&)
{
    // do nothing
}
