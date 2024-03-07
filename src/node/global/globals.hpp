#pragma once
#include "config/config.hpp"
#include <memory>

class BatchRegistry;
class HTTPEndpoint;
class PeerServer;
class ChainServer;
class Eventloop;
class UV_Helper;
namespace spdlog {
class logger;
}
namespace uvw{
    class loop;
}

struct Global {
    std::shared_ptr<uvw::loop>* uv_loop;
    ChainServer* chainServer;
    PeerServer* peerServer;
    UV_Helper* conman;
    Eventloop* core;
    BatchRegistry* batchRegistry;
    HTTPEndpoint* httpEndpoint;
    std::shared_ptr<spdlog::logger> connLogger;
    std::shared_ptr<spdlog::logger> syncdebugLogger;
    std::optional<Config> conf;
};

const Global& global();
HTTPEndpoint& http_endpoint();
inline spdlog::logger& connection_log() { return *global().connLogger; }
inline spdlog::logger& syncdebug_log() { return *global().syncdebugLogger; }
const Config& config();
int init_config(int argc, char** argv);
void global_init(std::shared_ptr<uvw::loop>* uv_loop, BatchRegistry* pbr, PeerServer* pps, ChainServer* pcs, UV_Helper* pcm, Eventloop* pel, HTTPEndpoint* httpEndpoint);
void start_global_services();
