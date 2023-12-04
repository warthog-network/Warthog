#pragma once
#include "config/config.hpp"
#include <memory>

class BatchRegistry;
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
    std::shared_ptr<spdlog::logger> connLogger;
    Config conf;
};

const Global& global();
inline spdlog::logger& connection_log() { return *global().connLogger; }
const Config& config();
int init_config(int argc, char** argv);
void global_init(BatchRegistry* pbr, PeerServer* pps, ChainServer* pcs, Conman* pcm, Eventloop* pel);
