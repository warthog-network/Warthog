#include "api_call.hpp"
#include "crypto/crypto.hpp"
#include "general/hex.hpp"
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"
#include <iostream>
using namespace std;
using namespace nlohmann;

API::API(std::string host, uint16_t port)
    : cli(host, port) {};
size_t writeFunction(void* ptr, size_t size, size_t nmemb, std::string* data)
{
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

std::string API::http_get(const std::string& path)
{
    if (auto res = cli.Get(path)) {
        return res->body;
    }
    throw std::runtime_error("API request failed. Are you running the node with RPC endpoint enabled?");
}

std::string API::http_post(const std::string& path, const std::string& postdata)
{
    if (auto res = cli.Post(path, postdata, ""s)) {
        return res->body;
    }
    throw std::runtime_error("API request failed. Are you running the node with RPC endpoint enabled?");
}

std::pair<std::string, int> API::submit_block(const Block& mt)
{
    std::pair<std::string, int> out;
    std::string path = "/chain/append";
    json j;
    j["body"] = serialize_hex(mt.body.data());
    j["header"] = serialize_hex(mt.header);
    j["height"] = mt.height.value();

    std::string b = j.dump();
    while (true) {
        try {
            std::string result = http_post(path, b);
            json parsed = json::parse(result);
            out.second = parsed["code"].get<int32_t>();
            if (out.second != 0) {
                out.first = parsed["error"].get<std::string>();
            }
            return out;
        } catch (std::runtime_error& e) {
            spdlog::error(e.what());
            spdlog::warn("Could not supply block, retrying in 100 milliseconds...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

Block API::get_mining(const Address& a)
{
    std::string url = "/chain/mine/" + a.to_string();
    while (true) {
        try {
            std::string out = http_get(url);

            json parsed;
            try {
                parsed = json::parse(out);
            } catch (...) {
                throw std::runtime_error("API request failed, response is malformed. Is the node version compatible with this wallet?");
            }
            int32_t code = parsed["code"].get<int32_t>();
            if (code != 0) {
                throw std::runtime_error("API request failed: " + parsed["error"].get<std::string>());
            }
            return Block {
                .height = Height(parsed["data"]["height"].get<uint32_t>()).nonzero_throw(EZEROHEIGHT),
                .header = hex_to_arr<80>(parsed["data"]["header"].get<std::string>()),
                .body = hex_to_vec(parsed["data"]["body"].get<std::string>()),
            };
        } catch (std::runtime_error& e) {
            spdlog::error(e.what());
            spdlog::warn("Could not get mining information, retrying in 100 milliseconds...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}
