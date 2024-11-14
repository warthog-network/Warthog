#include "globals.hpp"
#include "general/is_testnet.hpp"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/spdlog.h"
#include "transport/tcp/conman.hpp"
namespace {

std::string logdir()
{
    if (is_testnet()) {
        return "logs_testnet";
    } else {
        return "logs";
    }
}
auto create_connection_logger()
{
    auto max_size = 1048576 * 5; // 5 MB
    auto max_files = 3;
    auto res{ spdlog::rotating_logger_mt("connection_logger", config().get_default_datadir() + logdir() + "/connections.log", max_size, max_files)};
    res->flush_on(spdlog::level::info);
    return res;
}

auto create_syncdebug_logger()
{
    auto max_size = 1048576 * 5; // 5 MB
    auto max_files = 3;
    return spdlog::rotating_logger_mt("syncdebug_logger", config().get_default_datadir() + logdir() + "/syncdebug.log", max_size, max_files);
}

auto create_timing_logger()
{
    auto max_size = 1048576 * 50; // 50 MB
    auto max_files = 10;
    return spdlog::rotating_logger_mt("timing", config().get_default_datadir() + logdir() + "/timing.log", max_size, max_files);
}
auto create_longrunning_logger()
{
    auto max_size = 1048576 * 50; // 50 MB
    auto max_files = 10;
    return spdlog::rotating_logger_mt("longrunning", config().get_default_datadir() + logdir() + "/longrunning.log", max_size, max_files);
}

}

#ifdef DISABLE_LIBUV
#include "emscripten/proxying.h"
emscripten::ProxyingQueue globalProxyingQueue;
#endif

namespace {
Global globalinstance;
}

const Global& global()
{
    return globalinstance;
}

int init_config(int argc, char** argv)
{
    auto params { ConfigParams::from_args(argc, argv) };
    if (!params.has_value())
        return params.error();
    globalinstance.conf.emplace(std::move(params.value()));
    return 1;
};

const Config& config()
{
    return *globalinstance.conf;
}

#ifndef DISABLE_LIBUV
HTTPEndpoint& http_endpoint()
{
    return *globalinstance.httpEndpoint;
};

void global_init(BatchRegistry* pbr, PeerServer* pps, ChainServer* pcs, TCPConnectionManager* pcm, WSConnectionManager* wcm, Eventloop* pel, HTTPEndpoint* httpEndpoint)
#else
void global_init(BatchRegistry* pbr, PeerServer* pps, ChainServer* pcs, Eventloop* pel)
#endif
{
#ifndef DISABLE_LIBUV
    globalinstance.conman = pcm;
    globalinstance.wsconman = wcm;
    globalinstance.httpEndpoint = httpEndpoint;
#endif
    globalinstance.batchRegistry = pbr;
    globalinstance.peerServer = pps;
    globalinstance.chainServer = pcs;
    globalinstance.core = pel;
    globalinstance.connLogger = create_connection_logger();
    globalinstance.syncdebugLogger = create_syncdebug_logger();
    globalinstance.timingLogger.emplace(create_timing_logger(), create_longrunning_logger());
};

std::atomic<bool> shutdownSignal { false };
