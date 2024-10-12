#pragma once

#ifndef DISABLE_LIBUV
#define UWS_NO_ZLIB
#include "config.hpp"
#include "api/types/all.hpp"
#include "block/block.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
#include "connect_request.hpp"
#include "uwebsockets/App.h"
#include <thread>
#include <variant>

class WSSession;
class PeerServer;

class WSConnectionManager {
    // Events
    struct Shutdown {
        Error reason;
    };
    // struct Connect {
    //     WSConnectRequest conreq;
    // };
    struct Send {
        std::weak_ptr<WSSession> session;
        std::unique_ptr<char[]> data;
        size_t size;
    };
    struct Close {
        std::weak_ptr<WSSession> session;
        Error reason;
    };
    struct StartRead {
        std::weak_ptr<WSSession> session;
    };
    using Event = std::variant<
        // Connect, 
        Shutdown, Send, Close, StartRead>;

public:
    void start_read(std::weak_ptr<WSSession> p)
    {
        push_event(StartRead { std::move(p) });
    }
    void close(std::weak_ptr<WSSession> p, Error e)
    {
        push_event(Close { std::move(p), e });
    }
    WSConnectionManager(PeerServer& peerServer, WebsocketServerConfig);
    ~WSConnectionManager();
    void start();
    void shutdown(Error reason)
    {
        push_event(Shutdown { reason });
    }
    void wait_for_shutdown();
    // void connect(WSConnectRequest r) { push_event(Connect { std::move(r) }); }
    void async_send(std::weak_ptr<WSSession> session, std::unique_ptr<char[]> p, size_t N)
    {
        push_event(Send { std::move(session), std::move(p), N });
    };

private:
    void push_event(Event e);
    void create_context();
    void wakeup();

    void work();
    void process_events();
    void handle_event(Shutdown&&);
    // void handle_event(Connect&&);
    void handle_event(Send&&);
    void handle_event(Close&&);
    void handle_event(StartRead&&);

private:
    struct StartOptions {
        uint16_t port;
    };

    PeerServer& peerServer;

public:
    const WebsocketServerConfig config;

private:
    std::mutex m;
    struct lws_context* context = nullptr;
    std::vector<Event> events;
    std::thread worker;

    bool _shutdown = false;
};
#endif
