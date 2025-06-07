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

bool Endpoint::http_get(const std::string& get, std::string& out)
{
    httplib::Client cli(host, port);
    cli.set_read_timeout(10);
    if (auto res = cli.Get(get)) {
        out = std::move(res->body);
        return true;
    }
    return false;
}

int Endpoint::http_post(const std::string& path, const std::vector<uint8_t>& postdata, std::string& out)
{
    httplib::Client cli(host, port);
    cli.set_read_timeout(10);
    if (auto res = cli.Post(path, (const char*)postdata.data(), postdata.size(), ""s)) {
        out = std::move(res->body);
        return true;
    }
    return false;
}

std::pair<PinHeight, PinHash> Endpoint::get_pin()
{
    std::string out;
    std::string url = "/chain/head";
    if (!http_get(url, out)) {
        throw failed_msg();
    }
    try {
        json parsed = json::parse(out);
        std::string h = parsed["data"]["pinHash"].get<std::string>();
        auto pinHeight = Height(parsed["data"]["pinHeight"].get<int32_t>()).pin_height();
        auto pinHash{Hash::parse_string(h)};
        if (pinHeight && pinHash)
            return make_pair(*pinHeight, PinHash(*pinHash));
    } catch (...) {
    }
    throw std::runtime_error("API request failed, response is malformed. Is the node version compatible with this wallet?");
}

Funds_uint64 Endpoint::get_balance(const std::string& account)
{
    std::string out;
    std::string url = "/account/" + account + "/balance";
    if (!http_get(url, out)) {
        throw failed_msg();
    }
    json parsed = json::parse(out);
    auto r { Funds_uint64::parse(parsed["data"]["balance"].get<string>()) };
    if (r) {
        return *r;
    }
    throw std::runtime_error("API request failed, response is malformed. Is the node version compatible with this wallet?");
}

auto Endpoint::send_transaction(const std::string& txjson) -> std::variant<TxHash, Error>
{
    cout << "=====DEBUG INFO TRANSACTION JSON=====\n"
         << txjson << "\n"
         << "=====================================" << endl;

    std::string out;
    std::optional<std::string> error;
    std::string url = "/transaction/add";

    if (!http_post(url, std::vector<uint8_t>(txjson.begin(), txjson.end()), out)) {
        throw failed_msg();
    }
    try {
        json parsed = json::parse(out);
        auto iter = parsed.find("error");
        if (iter != parsed.end() && !iter->is_null()) {
            error = iter->get<string>();
        }
        auto code { parsed["code"].get<int32_t>() };
        if (code != 0 || error.has_value()) {
            return Error { code, *error };
        } else {
            auto hex_str { parsed.at("data")
                               .at("txHash")
                               .get<std::string>() };
            return TxHash { Hash { hex_to_arr<32>(hex_str) } };
        }
    } catch (...) {
        throw std::runtime_error("API request failed, response is malformed. Is the node version compatible with this wallet?");
    }
}

std::runtime_error Endpoint::failed_msg()
{
    return std::runtime_error { "API request to host " + host + " at port " + std::to_string(port) + " failed. Are you running the node with RPC endpoint enabled?" };
};
