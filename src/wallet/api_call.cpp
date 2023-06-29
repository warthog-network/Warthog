#include "api_call.hpp"
#include "block/chain/height.hpp"
#include "general/hex.hpp"
#include "httplib.hpp"
#include "nlohmann/json.hpp"
#include <iostream>
using namespace std;
using namespace nlohmann;

size_t writeFunction(void* ptr, size_t size, size_t nmemb, std::string* data)
{
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

bool http_get(const std::string& get, std::string& out)
{
    httplib::Client cli("localhost", 3000);
    if (auto res = cli.Get(get)) {
        out = std::move(res->body);
        return true;
    }
    return false;
}

int http_post(const std::string& path, const std::vector<uint8_t>& postdata, std::string& out)
{
    httplib::Client cli("localhost", 3000);
    if (auto res = cli.Post(path, (const char*)postdata.data(), postdata.size(), ""s)) {
        out = std::move(res->body);
        return true;
    }
    return false;
}

std::pair<PinHeight, Hash> get_pin()
{
    std::string out;
    std::string url = "/chain/head";
    if (!http_get(url, out)) {
        throw std::runtime_error("API request failed. Are you running the node with RPC endpoint enabled?");
    }
    try {
        json parsed = json::parse(out);
        std::string h = parsed["data"]["pinHash"].get<std::string>();
        auto pinHeight = Height(parsed["data"]["pinHeight"].get<int32_t>()).pin_height();
        Hash pinHash;
        if (pinHeight && parse_hex(h, pinHash))
            return make_pair(*pinHeight, pinHash);
    } catch (...) {
    }
    throw std::runtime_error("API request failed, response is malformed. Is the node version compatible with this wallet?");
}

Funds get_balance(const std::string& account)
{
    std::string out;
    std::string url = "/account/" + account + "/balance";
    if (!http_get(url, out)) {
        throw std::runtime_error("API request failed. Are you running the node with RPC endpoint enabled?");
    }
    json parsed = json::parse(out);
    auto r { Funds::parse(parsed["data"]["balance"].get<string>()) };
    if (r) {
        return *r;
    }
    throw std::runtime_error("API request failed, response is malformed. Is the node version compatible with this wallet?");
}

int32_t send_transaction(const std::vector<uint8_t>& txdata, std::string* error)
{
    std::string out;
    std::string url = "/transaction/add";
    if (!http_post(url, txdata, out)) {
        throw std::runtime_error("API request failed. Are you running the node with RPC endpoint enabled?");
    }
    try {
        json parsed = json::parse(out);
        if (error) {
            auto iter = parsed.find("error");
            if (iter != parsed.end() && !iter->is_null()) {
                *error = iter->get<string>();
            }
        }
        return parsed["code"].get<int32_t>();
    } catch (...) {
        throw std::runtime_error("API request failed, response is malformed. Is the node version compatible with this wallet?");
    }
}
