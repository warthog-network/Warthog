#pragma once

#include "general/page.hpp"
#include "ban_cache.hpp"
#include "db/peer_db.hpp"
#include "expected.hpp"
#include "general/errors.hpp"
#include "general/tcp_util.hpp"
#include "spdlog/spdlog.h"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>
#include <uv.h>
#include <variant>

class Connection;
class Conman;
struct Inspector;
struct Config;

class PeerServer {
public:
    struct Offense {
        IPv4 ip;
        int32_t offense;
        int64_t rowid;
    };
    using BannedCB = std::function<void(const std::vector<PeerDB::BanEntry>&)>;
    using OffensesCb = std::function<void(const tl::expected<std::vector<OffenseEntry>, int32_t>&)>;
    using ResultCB = std::function<void(const tl::expected<void, int32_t>&)>;

private:
    friend struct Inspector;
    struct NewConnection {
        Conman& cm;
        std::weak_ptr<Connection> c;
    };
    struct Unban {
        ResultCB cb;
    };

    struct GetOffenses {
        Page page;
        OffensesCb cb;
    };

public:
    bool async_register_close(IPv4 ip, int32_t offense, int64_t rowid = -1)
    {
        if (offense == EREFUSED)
            return false;
        return async_event(Offense { ip, offense, rowid });
    }
    void async_shutdown()
    {
        std::unique_lock<std::mutex> l(mutex);
        shutdown = true;
        hasWork = true;
        cv.notify_one();
    }
    bool async_validate(Conman& cm, std::shared_ptr<Connection> c)
    {
        // make sure that addref is called before
        return async_event(NewConnection { cm, std::move(c) });
    }
    bool async_get_banned(BannedCB cb)
    {
        return async_event(cb);
    };
    bool async_unban(ResultCB cb)
    {
        return async_event(Unban { std::move(cb) });
    }
    bool async_get_offenses(Page page, OffensesCb cb)
    {
        return async_event(GetOffenses { page, std::move(cb) });
    }
    bool async_register_peer(EndpointAddress a)
    {
        return async_event(RegisterPeer { a });
    }
    bool async_seen_peer(EndpointAddress a)
    {
        return async_event(SeenPeer { a });
    }
    bool async_get_recent_peers(
        std::function<void(std::vector<std::pair<EndpointAddress, uint32_t>>&&)>&& cb,
        size_t maxEntries = 100)
    {
        return async_event(GetRecentPeers { std::move(cb), maxEntries });
    }
    bool async_inspect(std::function<void(const PeerServer&)>&& cb)
    {
        return async_event(Inspect { std::move(cb) });
    }

    PeerServer(PeerDB& db, const Config&);
    ~PeerServer()
    {
        async_shutdown();
        worker.join();
    }

private:
    struct RegisterPeer {
        EndpointAddress a;
    };
    struct SeenPeer {
        EndpointAddress a;
    };
    struct GetRecentPeers {
        std::function<void(std::vector<std::pair<EndpointAddress, uint32_t>>&&)> cb;
        size_t maxEntries;
    };
    struct Inspect {
        std::function<void(const PeerServer&)> cb;
    };
    using Event = std::variant<Offense, NewConnection, GetOffenses, Unban, BannedCB, RegisterPeer, SeenPeer, GetRecentPeers, Inspect>;
    [[nodiscard]] bool async_event(Event e)
    {
        std::unique_lock<std::mutex> l(mutex);
        if (shutdown)
            return false;
        events.push(std::move(e));
        hasWork = true;
        cv.notify_one();
        return true;
    }
    void work();
    void accept_connection();
    void register_close(IPv4 address, ErrorTimestamp et, int64_t rowid);
    ////////////////
    //
    // private variables
    PeerDB& db;
    uint32_t now;
    BanCache bancache;
    void handle_event(Offense&&);
    void handle_event(Unban&&);
    void handle_event(GetOffenses&&);
    void handle_event(NewConnection&&);
    void handle_event(BannedCB&&);
    void handle_event(RegisterPeer&&);
    void handle_event(SeenPeer&&);
    void handle_event(GetRecentPeers&&);
    void handle_event(Inspect&&);

    ////////////////
    // Mutex protected variables
    std::mutex mutex;
    bool hasWork = false;
    bool enableBan;
    bool shutdown = false;
    std::queue<Event> events;
    std::condition_variable cv;

    // worker
    std::thread worker;
};
