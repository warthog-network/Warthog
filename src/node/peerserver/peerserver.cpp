#include "peerserver.hpp"
#include "asyncio/conman.hpp"
#include "asyncio/connection.hpp"
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

void PeerServer::start_request(const ConnectRequest& r)
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
    connectionSchedule.outbound_failed(fc.connectRequest);
    // TODO
};
