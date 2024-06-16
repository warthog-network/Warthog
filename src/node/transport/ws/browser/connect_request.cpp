#include "connect_request.hpp"
#include "connection.hpp"
#include "global/emscripten_proxy.hpp"
#include "spdlog/spdlog.h"

void WSBrowserConnectRequest::connect()
{
    proxy_to_main_runtime([req = *this]() {
        if (!WSConnection::connect(req)) {
            spdlog::warn("Cannot establish websocket connection to {}", req.address.to_string());
            // global().core->on_failed_connect(r, Error(ESTARTWEBSOCK)); // TODO: restart connect request later
        }
    });
}
