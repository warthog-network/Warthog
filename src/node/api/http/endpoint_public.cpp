#include "endpoint_public.hpp"
#include "api/http/parse.hpp"
#include "api/types/all.hpp"
#include "chainserver/transaction_ids.hpp"
#include "communication/mining_task.hpp"
#include "general/hex.hpp"
#include "json.hpp"
#include "spdlog/spdlog.h"
#include "version.hpp"
#include <charconv>
#include <iostream>
#include <type_traits>
using namespace std::placeholders;

namespace {
struct ParameterParser {
    std::string_view sv;
    template <typename T>
    requires std::is_integral_v<T>
    operator T()
    {
        T res {};
        auto result = std::from_chars(sv.data(), sv.end(), res);
        if (result.ec != std::errc {} || result.ptr != sv.end()) {
            throw Error(EMALFORMED);
        }
        return res;
    }
    operator API::HeightOrHash()
    {
        if (sv.length() == 64)
            return { Hash { *this } };
        return { Height { *this } };
    }
    operator Funds()
    {
        return Funds::throw_parse(sv);
    }
    operator Page()
    {
        return static_cast<uint32_t>(*this);
    }
    operator Hash()
    {
        return hex_to_arr<32>(sv);
    }
    operator NonzeroHeight()
    {
        return Height(static_cast<uint32_t>(*this)).nonzero_throw(EBADHEIGHT);
    }
    operator Height()
    {
        return Height(static_cast<uint32_t>(*this));
    }
    operator Address()
    {
        return Address(sv);
    }
};
void send_json(uWS::HttpResponse<false>* res, const std::string& s)
{
    res->writeHeader("Content-type", "application/json; charset=utf-8");
    res->end(s, true);
}

void nav(uWS::HttpResponse<false>* res, uWS::HttpRequest*)
{
    res->writeHeader("Content-type", "text/html; charset=utf-8");
    constexpr const char s[] = R"HTML(
<!doctype html>
<html>
    <head>
        <meta charset="utf-8" />
        <title>endpoint methods</title>
    </head>
    <body>
        <h1>Public API for Warthog node version )HTML" CMDLINE_PARSER_VERSION
                               R"HTML(</h1>
        <h2>Transaction endpoints</h2>
        <ul>
            <li>POST <a href=/transaction/add>/transaction/add</a> </li>
            <li>GET <a href=/transaction/mempool>/transaction/mempool</a></li>
            <li>GET <a href=/transaction/lookup/:txid>/transaction/lookup/:txid </a></li>
            <li>GET <a href=/transaction/latest>/transaction/lookup/latest </a></li>
        </ul>
        <h2>Chain endpoints</h2>
        <ul>
            <li>GET <a href=/chain/head>/chain/head</a></li>
            <li>GET <a href=/chain/block/:id/hash>/chain/block/:id/hash</a></li>
            <li>GET <a href=/chain/block/:id/header>/chain/block/:id/header</a></li>
            <li>GET <a href=/chain/block/:id>/chain/block/:id</a></li>
            <li>GET <a href=/chain/mine/:address>/chain/mine/:address</a></li>
            <li>POST <a href=/chain/append>/chain/append</a></li>
        </ul>
        <h2>Account endpoints</h2>
        <ul>
            <li>GET <a href=/account/:account/balance>/account/:account/balance</a></li>
            <li>GET <a href=/account/:account/history/:beforeTxIndex>/account/:account/history/:beforeTxIndex</a></li>
            <li>GET <a href=/account/richlist>/account/richlist</a></li>
        </ul>
        <h2>Tools endpoints</h2>
        <ul>
            <li>GET <a href=/tools/encode16bit/from_e8/:feeE8>/tools/encode16bit/from_e8/:feeE8</a></li>
            <li>GET <a href=/tools/encode16bit/from_string/:feestring>/tools/encode16bit/from_string/:feestring</a></li>
            <li>GET <a href=/tools/version>/tools/version</a></li>
        </ul>
    </body>
</html>
    )HTML";
    res->end(s, true);
}
} // namespace

void HTTPEndpointPublic::work()
{
    app.get("/", &nav);

    // transaction endpoints
    post("/transaction/add", parse_payment_create, put_mempool);
    get("/transaction/mempool", get_mempool);
    get_1("/transaction/lookup/:txid", lookup_tx);
    get("/transaction/latest", get_latest_transactions);

    // Chain endpoints
    get("/chain/head", get_block_head, jsonmsg::serialize<API::Head>);
    get_1("/chain/block/:id/hash", get_chain_hash);
    get_1("/chain/block/:id/header", get_chain_header);
    get_1("/chain/block/:id", get_chain_block);
    get_1("/chain/mine/:account", get_chain_mine);
    get_1("/chain/mine/:account/log", get_chain_mine_log);
    post("/chain/append", parse_mining_task, put_chain_append);

    // Account endpoints
    get_1("/account/:account/balance", get_account_balance);
    get_2("/account/:account/history/:beforeTxIndex", get_account_history);
    get("/account/richlist", get_account_richlist);

    // tools endpoints
    get_1("/tools/encode16bit/from_e8/:feeE8", get_round16bit_e8);
    get_1("/tools/encode16bit/from_string/:string", get_round16bit_funds);
    get("/tools/version", get_version);

    app.listen(bind.ipv4.to_string(), bind.port, std::bind(&HTTPEndpointPublic::on_listen, this, _1));
    lc.loop->run();
}

std::unique_ptr<HTTPEndpointPublic> HTTPEndpointPublic::make_endpoint_public(const Config& c)
{
    if (c.publicAPI)
        return std::make_unique<HTTPEndpointPublic>(Token {}, c);
    return {};
};
HTTPEndpointPublic::HTTPEndpointPublic(Token, const Config& c)
    : bind(c.publicAPI.value().bind)
    , app(lc.loop)
{
    spdlog::info("Public endpoint is {}.", bind.to_string());
    t = std::thread(&HTTPEndpointPublic::work, this);
}

void HTTPEndpointPublic::get(std::string pattern, auto asyncfun, auto serializer)
{
    app.get(pattern,
        [this, asyncfun, serializer, pattern](auto* res, auto* req) {
            spdlog::debug("GET {}", req->getUrl());
            asyncfun(
                [this, res, serializer](auto& data) {
                    async_reply(res, serializer(data));
                });
            pendingRequests.insert(res);
            res->onAborted([this, res]() { on_aborted(res); });
        });
}

void HTTPEndpointPublic::get(std::string pattern, auto asyncfun)
{
    app.get(pattern,
        [this, asyncfun, pattern](auto* res, auto* req) {
            spdlog::debug("GET {}", req->getUrl());
            asyncfun(
                [this, res](auto& data) {
                    async_reply(res, jsonmsg::serialize(data));
                });
            pendingRequests.insert(res);
            res->onAborted([this, res]() { on_aborted(res); });
        });
}

void HTTPEndpointPublic::get_1(std::string pattern, auto asyncfun)
{
    app.get(pattern,
        [this, asyncfun, pattern](auto* res, auto* req) {
            spdlog::debug("GET {}", req->getUrl());
            try {
                ParameterParser p1 { req->getParameter(0) };
                asyncfun(p1,
                    [this, res](auto& data) {
                        async_reply(res, jsonmsg::serialize(data));
                    });
                pendingRequests.insert(res);
                res->onAborted([this, res]() { on_aborted(res); });
            } catch (Error e) {
                send_json(res, jsonmsg::serialize(tl::make_unexpected(e.e)));
            }
        });
}
void HTTPEndpointPublic::get_2(std::string pattern, auto asyncfun)
{
    app.get(pattern,
        [this, asyncfun, pattern](auto* res, auto* req) {
            spdlog::debug("GET {}", req->getUrl());
            try {
                ParameterParser p1 { req->getParameter(0) };
                ParameterParser p2 { req->getParameter(1) };
                asyncfun(p1, p2,
                    [this, res](auto& data) {
                        async_reply(res, jsonmsg::serialize(data));
                    });
                pendingRequests.insert(res);
                res->onAborted([this, res]() { on_aborted(res); });
            } catch (Error e) {
                send_json(res, jsonmsg::serialize(tl::make_unexpected(e.e)));
            }
        });
}

void HTTPEndpointPublic::post(std::string pattern, auto parser, auto asyncfun)
{
    app.post(pattern,
        [this, pattern, parser = std::move(parser), asyncfun = std::move(asyncfun)](auto* res, uWS::HttpRequest* req) {
            spdlog::debug("POST {}", req->getUrl());
            std::vector<uint8_t> body;

            pendingRequests.insert(res);
            res->onData(
                [this, asyncfun = std::move(asyncfun), parser = std::move(parser), res, body = std::move(body)](std::string_view data, bool last) mutable {
                    body.insert(body.end(), data.begin(), data.end());
                    if (last) {
                        try {
                            asyncfun(parser(body),
                                [this, res](auto& data) {
                                    async_reply(res, jsonmsg::serialize(data));
                                });
                        } catch (Error e) {
                            auto ser = jsonmsg::serialize(tl::make_unexpected(e.e));
                            async_reply(res, ser);
                        }
                    }
                });
            res->onAborted([this, res]() { on_aborted(res); });
        });
}

void HTTPEndpointPublic::shutdown()
{
    bshutdown = true;
    if (listen_socket != nullptr) {
        us_listen_socket_close(0, listen_socket);
        listen_socket = nullptr;
    }
}

void HTTPEndpointPublic::on_event(WebsocketEvent&& e)
{
    std::visit([&](auto&& e) {
        handle_event(std::move(e));
    },
        std::move(e));
}

void HTTPEndpointPublic::handle_event(const API::Block& b)
{
    auto txt { jsonmsg::to_json(b).dump() };
    app.publish(b.WEBSOCKET_EVENT, txt, uWS::OpCode::TEXT);
}

void HTTPEndpointPublic::send_reply(uWS::HttpResponse<false>* res, const std::string& s)
{
    auto iter = pendingRequests.find(res);
    if (iter != pendingRequests.end()) {
        send_json(res, s);
        pendingRequests.erase(iter);
    }
}

void HTTPEndpointPublic::on_aborted(uWS::HttpResponse<false>* res)
{
    pendingRequests.erase(res);
}

void HTTPEndpointPublic::on_listen(us_listen_socket_t* ls)
{
    listen_socket = ls;
    if (listen_socket) {
        if (bshutdown) {
            us_listen_socket_close(0, listen_socket);
        }
    } else
        throw std::runtime_error("Cannot listen on " + bind.to_string());
}
