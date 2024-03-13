#pragma once

#include "asyncio/connection_base.hpp"
#include "ban_cache.hpp"
#include "db/peer_db.hpp"
#include "expected.hpp"
#include "general/errors.hpp"
#include "general/page.hpp"
#include "general/tcp_util.hpp"
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

namespace peerserver {
using Source = IPv4;
using time_point = std::chrono::steady_clock::time_point;
using duration = std::chrono::steady_clock::duration;
using steady_clock = std::chrono::steady_clock;

struct EndpointAddressItem {
    EndpointAddress address;
    Source source;
};

// data structure to encode success of recent connection tries
class ConnectionLog {
public:
    size_t recent_failures() const;
    [[nodiscard]] size_t log_failure(); // returns number of repeated failures
    void log_success();

private:
    uint32_t active_bits() const
    {
        return bits & 0x1F;
    }
    uint32_t bits { 0 };
};


enum class EndpointState {
    VERIFIED,
    UNVERIFIED_FAILED,
    UNVERIFIED_NEW
};

struct ReconnectContext {
    duration prevWait;
    EndpointState state;
    bool pinned;
};
class EndpointVector;
class EndpointData {
    friend class EndpointVector;

public:
    EndpointData(const EndpointAddressItem& i)
        : address(i.address)
        , sources { i.source }

    {
    }
    void add_source(Source);

    // returns expiration time point
    std::optional<time_point> try_pop(time_point, std::vector<ConnectRequest>& out);
    std::optional<time_point> timeout() const;

    // connection event callbacks
    void connection_established();
    void connection_closed();
    [[nodiscard]] time_point failed_connection_closed(const ReconnectContext&);
    [[nodiscard]] time_point outbound_connect_failed(const ReconnectContext&);

private:
    [[nodiscard]] time_point update_timer_failed(const ReconnectContext&);

    struct Timer {
        auto sleep_duration() const { return _sleepDuration; }
        auto timeout() const { return _timeout; }
        bool expired(time_point tp) const { return _timeout < tp; }
        void set(duration d)
        {
            _sleepDuration = d;
            _timeout = steady_clock::now() + d;
        }
        Timer()
        { // new timers wake up immediately
            set(duration::zero());
        }

    private:
        duration _sleepDuration;
        time_point _timeout;
    };
    EndpointAddress address;
    Timer timer;
    ConnectionLog connectionLog;
    std::set<Source> sources;
    bool pending { false };
    uint32_t connected { 0 };
};

using Sources = std::map<EndpointAddress, EndpointData>;

class EndpointVector {

public:
    [[nodiscard]] EndpointData* find(const EndpointAddress&) const;
    EndpointData* move_entry(const EndpointAddress& key, EndpointVector& to);
    std::pair<EndpointData&, bool> emplace(const EndpointAddressItem&);
    void pop_requests(time_point now, std::vector<ConnectRequest>&);
    bool set_timeout(const EndpointAddress&, time_point tp);

    std::optional<time_point> timeout() const { return wakeup_tp; }

private:
    EndpointData& insert(const EndpointAddressItem&);
    void update_wakeup_time(const std::optional<time_point>&);
    std::optional<time_point> wakeup_tp;
    mutable std::vector<EndpointData> data;
};

class ConnectionSchedule {
public:
    [[nodiscard]] std::optional<EndpointAddress> insert(EndpointAddressItem);

    void connection_established(const ConnectionData&);

    void outbound_closed(const ConnectionData&);
    void successful_outbound_closed(const ConnectRequest&);
    void failed_outbound_closed(const ConnectRequest&);
    void outbound_connect_failed(const ConnectRequest&);

    // void outbound_failed(const 

    [[nodiscard]] std::vector<peerserver::ConnectRequest> pop_requests();
    time_point wake_up_time();

private:
struct Found {
        EndpointData& item;
        EndpointState state;
};
    void refresh_wakeup_time();
    std::optional<ReconnectContext> get_reconnect_context(const ConnectRequest&);
    void update_wakeup_time(const std::optional<time_point> &);
    [[nodiscard]] auto find(const EndpointAddress& a)const  -> std::optional<Found> ;
    EndpointVector verified;
    EndpointVector unverifiedNew;
    EndpointVector unverifiedFailed;
    size_t totalConnected{0};
    std::set<EndpointAddress> pinned;
    std::optional<time_point> wakeup_tp;
};
}

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
    struct SuccessfulOutbound {
        std::shared_ptr<ConnectionBase> c;
    };
    using VerifyPeer = peerserver::EndpointAddressItem;
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
    void verify_peer(EndpointAddress c, IPv4 source)
    {
        async_event(VerifyPeer { std::move(c), source });
    }

    void notify_successful_outbound(std::shared_ptr<ConnectionBase> c)
    {
        async_event(SuccessfulOutbound(std::move(c)));
    }
    auto on_failed_connect(const peerserver::ConnectRequest& r, Error reason)
    {
        return async_event(FailedConnect { r, reason });
    };

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

    PeerServer(PeerDB& db, const ConfigParams&);
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
    struct FailedConnect {
        peerserver::ConnectRequest connectRequest;
        int32_t reason;
    };
    using Event = std::variant<SuccessfulOutbound, VerifyPeer, OnClose, Authenticate, GetOffenses, Unban, banned_callback_t, RegisterPeer, SeenPeer, GetRecentPeers, Inspect, FailedConnect>;
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
    void process_timer();
    void start_request(const peerserver::ConnectRequest&);
    void accept_connection();
    void register_close(IPv4 address, uint32_t now, int32_t offense, int64_t rowid);
    ////////////////
    //
    // private variables
    PeerDB& db;
    uint32_t now;
    BanCache bancache;
    void handle_event(SuccessfulOutbound&&);
    void handle_event(VerifyPeer&&);
    void handle_event(OnClose&&);
    void handle_event(Unban&&);
    void handle_event(GetOffenses&&);
    void handle_event(Authenticate&&);
    void handle_event(banned_callback_t&&);
    void handle_event(RegisterPeer&&);
    void handle_event(SeenPeer&&);
    void handle_event(GetRecentPeers&&);
    void handle_event(Inspect&&);
    void handle_event(FailedConnect&&);

    ////////////////
    // Mutex protected variables
    std::mutex mutex;
    bool hasWork = false;
    bool enableBan;
    bool shutdown = false;
    std::queue<Event> events;
    std::condition_variable cv;

    // std::vector<Pinned> pinned
    peerserver::ConnectionSchedule connectionSchedule;

    // worker
    std::thread worker;
};
