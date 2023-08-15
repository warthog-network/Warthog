#pragma once
#include "communication/mining_task.hpp"
#include "httplib.hpp"
#include <string>
#include <vector>

class Address;
struct API {
    API(std::string host, uint16_t port);
    [[nodiscard]] std::pair<std::string, int> submit_block(const Block& mt);
    [[nodiscard]] Block get_mining(const Address& a);

private:
    std::string http_get(const std::string& path);
    std::string http_post(const std::string& path, const std::string& postdata);
    httplib::Client cli;
};
