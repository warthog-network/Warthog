#pragma once
#include "config/config.hpp"
#include <memory>

class BatchRegistry;
class HTTPEndpoint;
class PeerServer;
class ChainServer;
class Eventloop;
class Conman;
namespace spdlog {
class logger;
}

struct Global {
    ChainServer* pcs;
    PeerServer* pps;
    Conman* pcm;
    Eventloop* pel;
    BatchRegistry* pbr;
    HTTPEndpoint* httpEndpoint;
    std::shared_ptr<spdlog::logger> connLogger;
    std::shared_ptr<spdlog::logger> syncdebugLogger;
    Config conf;
};

const Global& global();
HTTPEndpoint& http_endpoint();
inline spdlog::logger& connection_log() { return *global().connLogger; }
inline spdlog::logger& syncdebug_log() { return *global().syncdebugLogger; }
const Config& config();
int init_config(int argc, char** argv);
void global_init(BatchRegistry* pbr, PeerServer* pps, ChainServer* pcs, Conman* pcm, Eventloop* pel, HTTPEndpoint* httpEndpoint);
