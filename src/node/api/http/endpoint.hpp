#pragma once
#ifndef DISABLE_LIBUV
#define UWS_NO_ZLIB
#include "api/types/all.hpp"
#include "api/events/events.hpp"
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
    bool fresh{true};
    std::string inner;
};

class HTTPEndpoint {
public:
    using event_t = api::Event;
    static std::optional<HTTPEndpoint> make_public_endpoint(const ConfigParams&);
    HTTPEndpoint(TCPPeeraddr bind, bool isPublic = false);
    void start(){
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

private:
    void async_reply(uWS::HttpResponse<false>* res, std::string reply)
    {
        lc.loop->defer(std::bind(&HTTPEndpoint::send_reply, this, res, std::move(reply)));
    }
    void work();
    void shutdown();
    void on_event(event_t&& e);

    void send_reply(uWS::HttpResponse<false>* res, const std::string& s);
    void get(std::string pattern, auto asyncfun, auto serializer, bool priv = false);
    void get(std::string pattern, auto asyncfun, bool priv = false);
    void get_1(std::string pattern, auto asyncfun, bool priv = false);
    void get_2(std::string pattern, auto asyncfun, bool priv = false);
    void get_3(std::string pattern, auto asyncfun, bool priv = false);
    void post(std::string pattern, auto parser, auto asyncfun, bool priv = false);

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
