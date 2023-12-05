#include "globals.hpp"
#include "asyncio/conman.hpp"
#include "spdlog/sinks/rotating_file_sink.h"
namespace {

auto create_connection_logger()
{
    auto max_size = 1048576 * 5; // 5 MB
    auto max_files = 3;
    return spdlog::rotating_logger_mt("connection_logger", config().defaultDataDir + "logs/connections.log", max_size, max_files);
}

auto create_syncdebug_logger()
{
    auto max_size = 1048576 * 5; // 5 MB
    auto max_files = 3;
    return spdlog::rotating_logger_mt("syncdebug_logger", config().defaultDataDir + "logs/syncdebug.log", max_size, max_files);
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

void global_init(BatchRegistry* pbr, PeerServer* pps, ChainServer* pcs, Conman* pcm, Eventloop* pel)
{
    globalinstance.pbr = pbr;
    globalinstance.pps = pps;
    globalinstance.pcm = pcm;
    globalinstance.pcs = pcs;
    globalinstance.pel = pel;
    globalinstance.connLogger = create_connection_logger();;
    globalinstance.syncdebugLogger = create_syncdebug_logger();;
};
