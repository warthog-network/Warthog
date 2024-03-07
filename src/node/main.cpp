#include "api/http/endpoint.hpp"
#include "api/stratum/stratum_server.hpp"
#include "asyncio/conman.hpp"
#include "chainserver/server.hpp"
#include "db/chain_db.hpp"
#include "db/peer_db.hpp"
#include "eventloop/eventloop.hpp"
#include "general/errors.hpp"
#include "global/globals.hpp"
#include "peerserver/peerserver.hpp"
#include "spdlog/spdlog.h"
#include "uvw.hpp"

#include <iostream>
using namespace std;

static void shutdown(int32_t reason)
{
    global().conman->shutdown(reason);
    global().core->async_shutdown(reason);
    global().peerServer->async_shutdown();
}

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

#include <string>
static uv_signal_t sigint, sighup, sigterm;
void setup_signals(uv_loop_t* l)
{
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
    throw std::runtime_error("Cannot setup signals: " + std::string(errors::err_name(i)));
}
void free_signals()
{
    uv_close((uv_handle_t*)&sigint, nullptr);
    uv_close((uv_handle_t*)&sighup, nullptr);
    uv_close((uv_handle_t*)&sigterm, nullptr);
}

void initialize_srand()
{
    using namespace std::chrono;
    srand(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

struct ECC {
    ECC() { ECC_Start(); }
    ~ECC() { ECC_Stop(); }
};

int main(int argc, char** argv)
{
    ECC ecc;
    initialize_srand();
    int i = init_config(argc, argv);
    if (i <= 0)
        return i; // >0 means continue with execution
    BatchRegistry breg;

    spdlog::info("Chain database: {}", config().data.chaindb);
    spdlog::info("Peers database: {}", config().data.peersdb);

    // spdlog::flush_on(spdlog::level::debug);
    /////////////////////
    // uv loop
    auto l { uvw::loop::create() };

    spdlog::debug("Opening peers database \"{}\"", config().data.peersdb);
    PeerDB pdb(config().data.peersdb);
    PeerServer ps(pdb, config());

    spdlog::debug("Opening chain database \"{}\"", config().data.chaindb);
    ChainDB db(config().data.chaindb);
    auto cs = ChainServer::make_chain_server(db, breg, config().node.snapshotSigner);

    std::optional<StratumServer> stratumServer;
    if (config().stratumPool) {
        stratumServer.emplace(config().stratumPool->bind);
    }
    Eventloop el(ps, *cs, config());
    UV_Helper cm(l, ps, config());

    // setup signals
    setup_signals(l->raw());

    spdlog::debug("Starting libuv loop");

    // starting endpoint
    HTTPEndpoint endpoint { config().jsonrpc.bind };
    auto endpointPublic { HTTPEndpoint::make_public_endpoint(config()) };

    // setup globals
    global_init(&l, &breg, &ps, &*cs, &cm, &el, &endpoint);

    // running eventloops
    el.start_async_loop();
    if ((i = l->run( uvw::loop::run_mode::DEFAULT)))
        goto error;
    free_signals();
    if ((i = l->run( uvw::loop::run_mode::DEFAULT)))
        goto error;
    l->close();
    return 0;
error:
    spdlog::error("libuv error:", errors::err_name(i));
    return i;
}
