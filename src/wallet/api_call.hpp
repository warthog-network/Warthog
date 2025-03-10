#pragma once
#include "crypto/hash.hpp"
#include "general/funds.hpp"
#include <array>
#include <string>
#include <variant>
#include <vector>
class PinHeight;
class Endpoint {
    std::string host;
    uint16_t port;

public:
    struct Error{
        int code;
        std::string message;
    };
    Endpoint(std::string host, uint16_t port)
        : host(host)
        , port(port) {};
    Funds_uint64 get_balance(const std::string& account);
    std::variant<TxHash, Error> send_transaction(const std::string& txjson);
    std::pair<PinHeight, Hash> get_pin();
private:
    bool http_get(const std::string& get, std::string& out);
    int http_post(const std::string& path, const std::vector<uint8_t>& postdata, std::string& out);
    std::runtime_error failed_msg();
};
