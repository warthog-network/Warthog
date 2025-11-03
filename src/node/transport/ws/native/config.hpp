#pragma once

#include <cstdint>
#include "wrt/optional.hpp"
#include <string>
struct WebsocketServerConfig {
    std::string certfile { "ws.cert" };
    std::string keyfile { "ws.key" };
    uint16_t port = 0;
    bool XFowarded = false;
    bool bindLocalhost = false;
};
