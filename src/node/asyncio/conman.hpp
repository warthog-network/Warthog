#pragma once
#include "helpers/per_ip_counter.hpp"
#include "peerserver/peerserver.hpp"
#include <list>
#include <set>

#define DEFAULT_BACKLOG 128

struct Config;
struct Inspector;
class Connection;
class PeerServer;
class Conman {
    static constexpr size_t max_conn_per_ip = 3;
    friend class Connection;
    friend class Reconnecter;
    friend class PeerServer;
    struct ReconnectTimer;
    friend struct Inspector;

private:
    //////////////////////////////
    // Callers (static libuv callback functions)
    static void new_connection_caller(uv_stream_t* server, int status);
    static void wakeup_caller(uv_async_t* handle);
    static void close_caller(uv_handle_t* handle);
    static void reconnect_caller(uv_timer_t* handle);
    static void reconnect_closed_cb(uv_handle_t* handle);

    //////////////////////////////
    // Private methods
    void on_connect(int status);
    void on_wakeup();
    void on_reconnect_wakeup(ReconnectTimer& t);
    void on_reconnect_closed(ReconnectTimer& t);

    // ip counting
    bool count(IPv4);
    void count_force(IPv4);
    void uncount(IPv4);

    // reference counting
    void unlink(Connection* const pcon);
    void addref(const char*);
    void unref(const char*);

    void async_send(Connection* pcon); // CALLED BY PROCESSING THREAD
    void async_delete(Connection* pcon); // POTENTIALLY CALLED BY OTHER THREAD
    void async_close(Connection* pcon, int32_t error); // POTENTIALLY CALLED BY OTHER THREAD
    void async_validate(Connection* c, bool accept, int64_t rowid); // CALLED BY OTHER THREAD

public:
    struct APIPeerdata {
        EndpointAddress address;
        uint32_t since;
    };
    using PeersCB = std::function<void(std::vector<APIPeerdata>)>;

    void async_get_peers(PeersCB cb)
    {
        async_add_event(GetPeers { std::move(cb) });
    }
    void async_connect(EndpointAddress a, std::optional<uint32_t> reconnectSleep = {})
    {
        async_add_event(Connect { a, reconnectSleep });
    }
    void async_inspect(std::function<void(const Conman&)>&& cb)
    {
        async_add_event(Inspect { std::move(cb) });
    }
    uv_loop_t* loop() { return server.loop; }

    Conman(uv_loop_t* l, PeerServer& peerdb, const Config&,
        int backlog = DEFAULT_BACKLOG);
    void connect(EndpointAddress, std::optional<uint32_t> reconnectSleep = 0);
    void close(int32_t reason);

private:
    PeerServer& peerServer;
    struct ReconnectTimer {
        Conman* conman;
        uv_timer_t uv_timer;
        EndpointAddress address;
        size_t nextReconnectSleep;
        std::list<ReconnectTimer>::iterator iter;
    };
    const EndpointAddress bindAddress;
    //--------------------------------------
    // data accessed by libuv thread
    PerIpCounter perIpCounter;
    std::set<Connection*> connections;
    std::list<ReconnectTimer> reconnectTimers;
    int refcount { 0 }; // count connections + tcp_handle + wakeup
    bool closing = false;
    uv_tcp_t server;
    uv_async_t wakeup;

    // MESSAGE QUEUE
    struct Delete {
        Connection* c;
    };
    struct Close {
        Connection* c;
        int32_t reason;
    };
    struct Send {
        Connection* c;
    };
    struct Validation {
        Connection* c;
        bool accept;
        int64_t rowid;
    };
    struct GetPeers {
        PeersCB cb;
    };
    struct Connect {
        EndpointAddress a;
        std::optional<uint32_t> reconnectSleep;
    };
    struct Inspect {
        std::function<void(const Conman&)> callback;
    };
    using Event = std::variant<Delete, Close, Send, Validation, GetPeers, Connect, Inspect>;
    void async_add_event(Event e)
    {
        std::unique_lock<std::mutex> lock(mutex);
        events.push(std::move(e));
        uv_async_send(&wakeup);
    }
    //--------------------------------------
    // uv-mutex  locked shared members (message queues for uv thread)
    std::mutex mutex;
    std::queue<Event> events;

    // handle_event functions
    void handle_event(Delete&&);
    void handle_event(Close&&);
    void handle_event(Send&&);
    void handle_event(Validation&&);
    void handle_event(GetPeers&&);
    void handle_event(Connect&&);
    void handle_event(Inspect&&);
};
