#include "chainserver/db/chain_db.hpp"
#include "chainserver/server.hpp"
#include "eventloop/eventloop.hpp"
#include "general/errors.hpp"
#include "global/globals.hpp"
#include "peerserver/peerserver.hpp"
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

static void shutdown(Error reason)
{
    shutdownSignal.store(true);
    global().core->shutdown(reason);
    global().chainServer->shutdown();
    global().peerServer->shutdown();
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
#ifndef DISABLE_LIBUV
    // uv loop
    auto l { uvw::loop::create() };
#endif

    spdlog::debug("Opening peers database \"{}\"", config().data.peersdb);
    PeerDB pdb(config().data.peersdb);
    PeerServer ps(pdb, config());

    spdlog::debug("Opening chain database \"{}\"", config().data.chaindb);
    ChainDB db(config().data.chaindb);
    auto cs = ChainServer::make_chain_server(db, breg, config().node.snapshotSigner);

    auto el { Eventloop::create(ps, *cs, config()) };

#ifndef DISABLE_LIBUV
    std::optional<StratumServer> stratumServer;
    if (config().stratumPool) {
        stratumServer.emplace(config().stratumPool->bind);
    }
    auto cm{ TCPConnectionManager::make_shared(l, ps, config())};
    WSConnectionManager wscm(ps, config().websocketServer);
    // setup signals
    setup_signals(l->raw());

    // starting endpoint
    HTTPEndpoint endpoint { config().jsonrpc.bind };
    auto endpointPublic { HTTPEndpoint::make_public_endpoint(config()) };

    global_init(&breg, &ps, &*cs, cm.get(), &wscm, el.get(), &endpoint);
#else
    global_init(&breg, &ps, &*cs, el.get());
#endif

    // setup globals

    // spawn new threads
    ps.start();
    cs->start();
    el->start();

#ifdef DISABLE_LIBUV
    virtual_endpoint_initialize();
    while (true) {
        sleep(1000); // don't shut down
    }
    return 0;
#else
    endpoint.start();
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
    spdlog::error("libuv error:", errors::err_name(i));
    return i;
#endif
}
