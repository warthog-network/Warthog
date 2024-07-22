#include "connect_request.hpp"
#include "connection.hpp"
#include "eventloop/eventloop.hpp"
#include "global/emscripten_proxy.hpp"
#include "global/globals.hpp"
#include "spdlog/spdlog.h"

void WSBrowserConnectRequest::connect() const
{
    proxy_to_main_runtime([req = *this]() {
        if (!WSConnection::connect(req)) {
            spdlog::warn("Cannot establish websocket connection to {}", req.address().to_string());
            global().core->on_failed_connect(req, Error(ESTARTWEBSOCK)); // TODO: restart connect request later
        }else{
            spdlog::info("Trying to establish websocket connection to {}", req.address().to_string());
        }
    });
}
