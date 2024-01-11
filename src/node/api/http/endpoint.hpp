#pragma once
#define UWS_NO_ZLIB
#include "api/types/all.hpp"
#include "block/block.hpp"
#include "general/tcp_util.hpp"
#include "uwebsockets/App.h"
#include <thread>
#include <variant>

using WebsocketEvent = std::variant<API::Block>;

struct Config;
class HTTPEndpoint {
public:
    HTTPEndpoint(const Config&);
    ~HTTPEndpoint()
    {
        lc.loop->defer(std::bind(&HTTPEndpoint::shutdown, this));
    }
    void push_event(WebsocketEvent e)
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
    void on_event(WebsocketEvent&& e);

    void send_reply(uWS::HttpResponse<false>* res, const std::string& s);
    void get(std::string pattern, auto asyncfun, auto serializer);
    void get(std::string pattern, auto asyncfun);
    void get_1(std::string pattern, auto asyncfun);
    void get_2(std::string pattern, auto asyncfun);
    void post(std::string pattern, auto parser, auto asyncfun);

    //////////////////////////////
    // handlers
    void on_aborted(uWS::HttpResponse<false>* res);
    void on_listen(us_listen_socket_t* ls);

    //////////////////////////////
    // handlers for websocket events
    void handle_event(const API::Block&);

    //////////////////////////////
    // variables
    std::set<uWS::HttpResponse<false>*> pendingRequests;
    EndpointAddress bind;
    us_listen_socket_t* listen_socket = nullptr;
    const uWS::LoopCleaner lc;
    uWS::App app;
    bool bshutdown = false;
    std::thread t;
};
