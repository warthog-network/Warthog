#pragma once
#include "crypto/hash.hpp"
#include "general/funds.hpp"
#include <array>
#include <string>
#include <vector>
struct PinHeight;
class Endpoint {
    std::string host;
    uint16_t port;

public:
    Endpoint(std::string host, uint16_t port)
        : host(host)
        , port(port) {};
    Funds get_balance(const std::string& account);
    int32_t send_transaction(const std::vector<uint8_t>& txdata, std::string* error);
    std::pair<PinHeight, Hash> get_pin();
private:
    bool http_get(const std::string& get, std::string& out);
    int http_post(const std::string& path, const std::vector<uint8_t>& postdata, std::string& out);
    std::runtime_error failed_msg();
};
