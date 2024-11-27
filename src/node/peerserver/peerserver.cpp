#include "peerserver.hpp"
#include "asyncio/conman.hpp"
#include "asyncio/connection.hpp"
#include "config/config.hpp"
#include "db/peer_db.hpp"
#include "general/error_time.hpp"
#include "general/now.hpp"

using namespace std::chrono_literals;
namespace {
std::optional<ErrorTimepoint> ban_data(Error offense)
{
    if (errors::leads_to_ban(offense))
        return ErrorTimepoint::from_duration(offense, 20min);
    return std::nullopt;
}
} // namespace

PeerServer::PeerServer(PeerDB& db, const Config& config)
    : db(db)
    , enableBan(config.peers.enableBan)
{
    worker = std::thread(&PeerServer::work, this);
}
void PeerServer::register_close(IPv4 address, ErrorTimestamp et, int64_t rowid)
{
    if (auto banData { ban_data(et.error) }) {
        db.set_ban(address, *banData);
        bancache.set(address, *banData);
        db.insert_offense(address, et.error);
    }
    if (rowid >= 0)
        db.insert_disconnect(rowid, et);
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
        }
    }
}

void PeerServer::handle_event(Offense&& o)
{
    register_close(o.ip, { o.offense, now }, o.rowid);
}

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

void PeerServer::handle_event(NewConnection&& nc)
{
    if (auto l { nc.c.lock() }; l) {
        bool allowed = true;
        const IPv4& ip = l->peer_address().ipv4;
        auto banState {
            [&]() -> std::optional<ErrorTimestamp> {
                if (auto v { bancache.get(ip) })
                    return static_cast<ErrorTimestamp>(*v);
                return db.get_ban_state(ip);
            }()
        };
        if (banState) { // found entry, must be in db
            if (enableBan == true && banState->timestamp > now) {
                allowed = false;
                db.insert_refuse(ip, now);
            }
        } else {
            db.insert_peer(ip);
        };
        if (allowed) {
            bancache.set(ip, ErrorTimepoint::from_duration(ECONNRATELIMIT, 30s));
            auto rowid = db.insert_connect(ip.data, now);
            nc.cm.async_validate(std::move(l), true, rowid);
        } else {
            nc.cm.async_validate(std::move(l), false, -1);
        }
    }
}

void PeerServer::handle_event(BannedCB&& cb)
{
    auto banned = db.get_banned_peers();
    cb(banned);
};
void PeerServer::handle_event(RegisterPeer&& e)
{
    db.peer_insert(e.a);
};
void PeerServer::handle_event(SeenPeer&& e)
{
    db.peer_seen(e.a, now);
};
void PeerServer::handle_event(GetRecentPeers&& e)
{
    e.cb(db.recent_peers(e.maxEntries));
};
void PeerServer::handle_event(Inspect&& e)
{
    e.cb(*this);
};
