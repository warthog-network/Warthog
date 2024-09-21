#pragma once
#ifndef DISABLE_LIBUV
#define UWS_NO_ZLIB
#include "api/events/events.hpp"
#include "api/events/subscription_fwd.hpp"
#include "api/types/all.hpp"
#include "block/block.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
#include "uwebsockets/App.h"
#include <thread>
#include <variant>

struct ConfigParams;
class IndexGenerator {
public:
    void get(std::string s);
    void post(std::string s);
    void section(std::string s);
    std::string result(bool isPublic) const;

private:
    bool fresh { true };
    std::string inner;
};


class HTTPEndpoint {
    template<typename T>
        friend class RouterHook;

public:
    static constexpr bool SSL = false;
    using event_t = api::Event;
    static std::optional<HTTPEndpoint> make_public_endpoint(const ConfigParams&);
    HTTPEndpoint(TCPPeeraddr bind, bool isPublic = false);
    void start()
    {
        assert(!worker.joinable());
        worker = std::thread(&HTTPEndpoint::work, this);
    }
    ~HTTPEndpoint()
    {
        lc.loop->defer(std::bind(&HTTPEndpoint::shutdown, this));
        if (worker.joinable())
            worker.join();
    }
    void push_event(event_t e)
    {
        lc.loop->defer([this, e = std::move(e)]() mutable {
            on_event(std::move(e));
        });
    };

    void send_event(std::vector<subscription_ptr> subscribers, subscription::events::Event&&);

private:
    void dispatch(std::vector<subscription_ptr> subscribers, std::string&& serialized);
    void async_reply(uWS::HttpResponse<false>* res, std::string reply)
    {
        lc.loop->defer(std::bind(&HTTPEndpoint::send_reply, this, res, std::move(reply)));
    }
    auto& router()
    {
        return app;
    }
    void insert_pending(uWS::HttpResponse<SSL>* res)
    {
        pendingRequests.insert(res);
        res->onAborted([this, res]() { on_aborted(res); });
    }
    void work();
    void shutdown();
    void on_event(event_t&& e);

    void send_reply(uWS::HttpResponse<false>* res, const std::string& s);
    static void reply_json(uWS::HttpResponse<false>* res, const std::string& s);

    //////////////////////////////
    // handlers
    void on_aborted(uWS::HttpResponse<false>* res);
    void on_listen(us_listen_socket_t* ls);

    //////////////////////////////
    // variables
    IndexGenerator indexGenerator;
    std::set<uWS::HttpResponse<false>*> pendingRequests;
    TCPPeeraddr bind;
    bool isPublic;
    us_listen_socket_t* listen_socket = nullptr;
    const uWS::LoopCleaner lc;
    uWS::App app;
    bool bshutdown = false;
    std::thread worker;
};
#endif
