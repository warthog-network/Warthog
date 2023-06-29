#pragma once
#define UWS_NO_ZLIB
#include "general/tcp_util.hpp"
#include "uwebsockets/App.h"
#include <thread>

struct Config;
class HTTPEndpoint {
public:
    HTTPEndpoint(const Config&);
    ~HTTPEndpoint()
    {
        lc.loop->defer(std::bind(&HTTPEndpoint::shutdown, this));
    }

private:
    void async_reply(uWS::HttpResponse<false>* res, std::string reply)
    {
        lc.loop->defer(std::bind(&HTTPEndpoint::send_reply, this, res, std::move(reply)));
    }
    void work();
    void shutdown();

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
    // variables
    std::set<uWS::HttpResponse<false>*> pendingRequests;
    EndpointAddress bind;
    us_listen_socket_t* listen_socket = nullptr;
    const uWS::LoopCleaner lc;
    uWS::App app;
    bool bshutdown = false;
    std::jthread t;
};
