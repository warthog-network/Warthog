#pragma once

#include <cstdint>
#include <optional>
#include <string>
struct WebsocketServerConfig {
    std::string certfile { "ws.cert" };
    std::string keyfile { "ws.key" };
    uint16_t port = 0;
    bool useProxy = false;
};
