#include "chainserver/db/chain_db.hpp"
#include "chainserver/server.hpp"
#include "communication/rxtx_server/rxtx_server.hpp"
#include "eventloop/eventloop.hpp"
#include "general/errors.hpp"
#include "general/logger/log_memory.hpp"
#include "global/globals.hpp"
#include "peerserver/peerserver.hpp"
#include "spdlog/sinks/ansicolor_sink.h"
#include "spdlog/sinks/callback_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include "transport/ws/native/ws_conman.hpp"

#ifdef DISABLE_LIBUV
#include "api/wasm/endpiont_wasm.hpp"
#include "config/browser.hpp"
#else
static void shutdown(Error reason);
#include "api/http/endpoint.hpp"
#include "api/stratum/stratum_server.hpp"
#include "transport/tcp/conman.hpp"
#include "uvw.hpp"
static void signal_caller(uv_signal_t* /*handle*/, int signum)
{
    spdlog::info("Terminating...");
    switch (signum) {
    case SIGTERM:
        shutdown(ESIGTERM);
        return;
    case SIGHUP:
        shutdown(ESIGHUP);
        return;
    case SIGINT:
        shutdown(ESIGINT);
        return;
    default:;
    }
}

static uv_signal_t sigint, sighup, sigterm;
void setup_signals(uv_loop_t* l)
{
#if defined(SIGPIPE)
    signal(SIGPIPE, SIG_IGN); // as per recommendation of https://docs.libuv.org/en/v1.x/_sources/guide/filesystem.rst.txt
#endif
    int i;
    if ((i = uv_signal_init(l, &sigint) < 0))
        goto error;
    if (uv_signal_start(&sigint, signal_caller, SIGINT))
        goto error;
    if ((i = uv_signal_init(l, &sighup) < 0))
        goto error;
    if (uv_signal_start(&sighup, signal_caller, SIGHUP))
        goto error;
    if ((i = uv_signal_init(l, &sigterm) < 0))
        goto error;
    if (uv_signal_start(&sigterm, signal_caller, SIGTERM))
        goto error;
    uv_unref((uv_handle_t*)&sigint);
    uv_unref((uv_handle_t*)&sighup);
    uv_unref((uv_handle_t*)&sigterm);
    return;
error:
    throw std::runtime_error("Cannot setup signals: " + std::string(Error(i).err_name()));
}
void free_signals()
{
    uv_close((uv_handle_t*)&sigint, nullptr);
    uv_close((uv_handle_t*)&sighup, nullptr);
    uv_close((uv_handle_t*)&sigterm, nullptr);
}

static void shutdown(Error reason)
{
    shutdownSignal.store(true);
    global().core->shutdown(reason);
    global().chainServer->shutdown();
    global().peerServer->shutdown();
    global().rxtxServer->shutdown();
#ifndef DISABLE_LIBUV
    global().conman->shutdown(reason);
    global().wsconman->shutdown(reason);
    global().wsconman->wait_for_shutdown();
#endif
    global().core->wait_for_shutdown();
    global().chainServer->wait_for_shutdown();
    global().peerServer->wait_for_shutdown();
}
#endif

int run_app(int argc, char** argv)
{
    int i = init_config(argc, argv);
    if (i <= 0)
        return i; // >0 means continue with execution
    BatchRegistry breg;

    // spdlog::set_default_logger
    spdlog::info("Chain database: {}", config().data.chaindb);
    spdlog::info("Peers database: {}", config().data.peersdb);
    spdlog::info("Rxtx database: {}", config().data.rxtxdb);
    spdlog::info("Minimal transaction fee: {} WART", config().minMempoolFee.load().to_string());

    // spdlog::flush_on(spdlog::level::debug);
#ifndef DISABLE_LIBUV
    // uv loop
    auto l { uvw::loop::create() };
#endif

    PeerDB pdb(config().data.peersdb);
    PeerServer ps(pdb, config());

    ChainDB db(config().data.chaindb);
    auto cs = ChainServer::make_chain_server(db, breg, config().node.snapshotSigner);

    rxtx::Server rxtxServer;

    auto el { Eventloop::create(ps, *cs, config()) };

#ifndef DISABLE_LIBUV
    std::optional<StratumServer> stratumServer;
    if (config().stratumPool) {
        stratumServer.emplace(*config().stratumPool);
    }
    auto cm { TCPConnectionManager::make_shared(l, ps, config()) };
    WSConnectionManager wscm(ps, config().websocketServer);
    // setup signals
    setup_signals(l->raw());

    // starting endpoint
    HTTPEndpoint endpoint { config().jsonrpc.bind };
    auto endpointPublic { HTTPEndpoint::make_public_endpoint(config()) };

    global_init(&breg, &rxtxServer, &ps, &*cs, cm.get(), &wscm, el.get(), &endpoint);
#else
    global_init(&breg, &rxtxServer, &ps, &*cs, el.get());
#endif

        // setup globals

        // spawn new threads
        ps.start();
    cs->start();
    el->start();
    rxtxServer.start();

#ifdef DISABLE_LIBUV
    virtual_endpoint_initialize();
    while (true) {
        sleep(1000); // don't shut down
    }
    return 0;
#else
    endpoint.start();
    if (endpointPublic)
        endpointPublic->start();
    wscm.start();
    if (stratumServer)
        stratumServer->start();
    spdlog::debug("Starting libuv loop");
    // running eventloop
    if ((i = l->run(uvw::loop::run_mode::DEFAULT)))
        goto error;
    free_signals();
    if ((i = l->run(uvw::loop::run_mode::DEFAULT)))
        goto error;
    l->close();
    return 0;
error:
    spdlog::error("libuv error: {}", Error(i).err_name());
    return i;
#endif
}

int main(int argc, char** argv)
{
    global_startup();
    auto i { run_app(argc, argv) };
    global_cleanup();
    return i;
}
