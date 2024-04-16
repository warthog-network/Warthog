#include "globals.hpp"
#include "asyncio/tcp/conman.hpp"
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
    return spdlog::rotating_logger_mt("connection_logger", config().get_default_datadir() + logdir() + "/connections.log", max_size, max_files);
}

auto create_syncdebug_logger()
{
    auto max_size = 1048576 * 5; // 5 MB
    auto max_files = 3;
    return spdlog::rotating_logger_mt("syncdebug_logger", config().get_default_datadir() + logdir() + "/syncdebug.log", max_size, max_files);
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

void global_init(std::shared_ptr<uvw::loop>* uv_loop, BatchRegistry* pbr, PeerServer* pps, ChainServer* pcs, UV_Helper* pcm, Eventloop* pel, HTTPEndpoint* httpEndpoint)
{
    globalinstance.uv_loop = uv_loop;
    globalinstance.batchRegistry = pbr;
    globalinstance.peerServer = pps;
    globalinstance.conman = pcm;
    globalinstance.chainServer = pcs;
    globalinstance.core = pel;
    globalinstance.httpEndpoint = httpEndpoint;
    globalinstance.connLogger = create_connection_logger();
    ;
    globalinstance.syncdebugLogger = create_syncdebug_logger();
    ;
};

HTTPEndpoint& http_endpoint()
{
    return *globalinstance.httpEndpoint;
};
