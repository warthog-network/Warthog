#pragma once

#include "transport/connection_base.hpp"
#include "ban_cache.hpp"
#include "db/peer_db.hpp"
#include "eventloop/address_manager/connection_schedule.hpp"
#include "expected.hpp"
#include "general/errors.hpp"
#include "general/page.hpp"
#include "transport/tcp/tcp_sockaddr.hpp"
#include "spdlog/spdlog.h"
#include <bitset>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <uv.h>
#include <variant>

class TCPConnection;
class UV_Helper;
struct Inspector;
struct ConfigParams;

class PeerServer {
public:
    struct OnClose {
        std::shared_ptr<peerserver::ConnectionData> con;
        int32_t offense;
    };
    using banned_callback_t = std::function<void(const std::vector<PeerDB::BanEntry>&)>;
    using offenses_callback_t = std::function<void(const tl::expected<std::vector<OffenseEntry>, int32_t>&)>;
    using result_callback_t = std::function<void(const tl::expected<void, int32_t>&)>;

private:
    friend struct Inspector;
    struct Authenticate {
        std::shared_ptr<ConnectionBase> c;
    };
    struct Unban {
        result_callback_t cb;
    };

    struct GetOffenses {
        Page page;
        offenses_callback_t cb;
    };

public:
    bool async_register_close(std::shared_ptr<peerserver::ConnectionData> con, int32_t offense)
    {
        // TODO:
        // global().core->async_report_failed_outbound(peerAddress);
        if (offense == EREFUSED)
            return false;
        return async_event(OnClose { std::move(con), offense });
    }
    void async_shutdown()
    {
        std::unique_lock<std::mutex> l(mutex);
        shutdown = true;
        hasWork = true;
        cv.notify_one();
    }

    bool authenticate(std::shared_ptr<ConnectionBase> c)
    {
        return async_event(Authenticate { std::move(c) });
    }

    bool async_get_banned(banned_callback_t cb)
    {
        return async_event(cb);
    };
    bool async_unban(result_callback_t cb)
    {
        return async_event(Unban { std::move(cb) });
    }
    bool async_get_offenses(Page page, offenses_callback_t cb)
    {
        return async_event(GetOffenses { page, std::move(cb) });
    }
    bool async_register_peer(TCPSockaddr a)
    {
        return async_event(RegisterPeer { a });
    }
    bool async_seen_peer(TCPSockaddr a)
    {
        return async_event(SeenPeer { a });
    }
    bool async_get_recent_peers(
        std::function<void(std::vector<std::pair<Sockaddr, uint32_t>>&&)>&& cb,
        size_t maxEntries = 100)
    {
        return async_event(GetRecentPeers { std::move(cb), maxEntries });
    }
    bool async_inspect(std::function<void(const PeerServer&)>&& cb)
    {
        return async_event(Inspect { std::move(cb) });
    }

    PeerServer(PeerDB& db, const ConfigParams&);
    ~PeerServer()
    {
        async_shutdown();
        if (worker.joinable())
            worker.join();
    }
    void start();

private:
    struct RegisterPeer {
        TCPSockaddr a;
    };
    struct SeenPeer {
        TCPSockaddr a;
    };
    struct GetRecentPeers {
        std::function<void(std::vector<std::pair<Sockaddr, uint32_t>>&&)> cb;
        size_t maxEntries;
    };
    struct Inspect {
        std::function<void(const PeerServer&)> cb;
    };
    using Event = std::variant<OnClose, Authenticate, GetOffenses, Unban, banned_callback_t, RegisterPeer, SeenPeer, GetRecentPeers, Inspect>;
    bool async_event(Event e)
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
    void register_close(IPv4 address, uint32_t now, int32_t offense, int64_t rowid);
    ////////////////
    //
    // private variables
    PeerDB& db;
    uint32_t now;
    BanCache bancache;
    void handle_event(OnClose&&);
    void handle_event(Unban&&);
    void handle_event(GetOffenses&&);
    void handle_event(Authenticate&&);
    void handle_event(banned_callback_t&&);
    void handle_event(RegisterPeer&&);
    void handle_event(SeenPeer&&);
    void handle_event(GetRecentPeers&&);
    void handle_event(Inspect&&);

    void on_close(const OnClose&, TCPSockaddr);

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
