#include "globals.hpp"
#include "asyncio/conman.hpp"
#include "general/is_testnet.hpp"
#include "spdlog/sinks/rotating_file_sink.h"
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
    return spdlog::rotating_logger_mt("connection_logger", config().defaultDataDir + logdir() + "/connections.log", max_size, max_files);
}

auto create_syncdebug_logger()
{
    auto max_size = 1048576 * 5; // 5 MB
    auto max_files = 3;
    return spdlog::rotating_logger_mt("syncdebug_logger", config().defaultDataDir + logdir() + "/syncdebug.log", max_size, max_files);
}

auto create_timing_logger()
{
    auto max_size = 1048576 * 50; // 50 MB
    auto max_files = 10;
    return spdlog::rotating_logger_mt("timing", config().defaultDataDir + logdir() + "/timing.log", max_size, max_files);
}
auto create_longrunning_logger()
{
    auto max_size = 1048576 * 50; // 50 MB
    auto max_files = 10;
    return spdlog::rotating_logger_mt("longrunning", config().defaultDataDir + logdir() + "/longrunning.log", max_size, max_files);
}

}

namespace {
Global globalinstance;
}

const Global& global()
{
    return globalinstance;
}

int init_config(int argc, char** argv)
{
    auto& conf = globalinstance.conf;
    if (int i = conf.init(argc, argv))
        return i;
    return 0;
};

const Config& config()
{
    return globalinstance.conf;
}
Config& set_config()
{
    return globalinstance.conf;
}

void global_init(BatchRegistry* pbr, PeerServer* pps, ChainServer* pcs, Conman* pcm, Eventloop* pel, HTTPEndpoint* httpEndpoint)
{
    globalinstance.pbr = pbr;
    globalinstance.pps = pps;
    globalinstance.pcm = pcm;
    globalinstance.pcs = pcs;
    globalinstance.pel = pel;
    globalinstance.httpEndpoint = httpEndpoint;
    globalinstance.connLogger = create_connection_logger();
    ;
    globalinstance.syncdebugLogger = create_syncdebug_logger();
    ;
    globalinstance.timingLogger.emplace(create_timing_logger(), create_longrunning_logger());
};

HTTPEndpoint& http_endpoint()
{
    return *globalinstance.httpEndpoint;
};
