#include "endpoint.hpp"
#include "api/http/parse.hpp"
#include "api/types/accountid_or_address.hpp"
#include "api/types/all.hpp"
#include "api/http/rest.hxx"
#include "chainserver/transaction_ids.hpp"
#include "communication/mining_task.hpp"
#include "general/hex.hpp"
#include "json.hpp"
#include "spdlog/spdlog.h"
#include "version.hpp"
#include <iostream>
#include <type_traits>
using namespace std::placeholders;

namespace {

void send_html(uWS::HttpResponse<false>* res, const std::string& s)
{
    res->writeHeader("Content-type", "text/html; charset=utf-8");
    res->end(s, true);
}
} // namespace

void IndexGenerator::get(std::string s)
{
    inner += "            <li>GET <a href=" + s + ">" + s + "</a></li>";
}
void IndexGenerator::post(std::string s)
{
    inner += "            <li>POST <a href=" + s + ">" + s + "</a></li>";
}
void IndexGenerator::section(std::string s)
{
    if (!fresh) {
        inner += "        </ul>";
    } else
        fresh = false;
    inner += R"(        <h2>)" + s + R"(</h2>
        <ul>)";
}
std::string IndexGenerator::result(bool isPublic) const
{
    return R"HTML(
<!doctype html>
<html>
    <head>
        <meta charset="utf-8" />
        <title>endpoint methods</title>
    </head>
    <body>
        <h1>)HTML"
        + std::string(isPublic ? "Public " : "") + "API for Warthog node version " CMDLINE_PARSER_VERSION + "</h1>\n"
        + inner
        +
        R"HTML(</ul>
    </body>
</html>)HTML";
}

void HTTPEndpoint::reply_json(uWS::HttpResponse<false>* res, const std::string& s){
    res->writeHeader("Content-type", "text/html; charset=utf-8");
    res->end(s, true);
}

void HTTPEndpoint::work()
{
    app.get("/", [&](uWS::HttpResponse<false>* res, uWS::HttpRequest*) {
        send_html(res, indexGenerator.result(isPublic));
    });

    api_t::hook_endpoints(*this);
    app.ws<int>("/ws/chain_delta", {
                                       .open = [](auto* ws) {
                                           ws->subscribe(api::Block::eventName);
                                           ws->subscribe(api::Rollback::eventName);
                                       },
                                   });
    app.listen(bind.ip.to_string(), bind.port, std::bind(&HTTPEndpoint::on_listen, this, _1));
    lc.loop->run();
}

std::optional<HTTPEndpoint> HTTPEndpoint::make_public_endpoint(const ConfigParams&)
{
    auto& pAPI { config().publicAPI };
    if (!pAPI)
        return {};
    return std::optional<HTTPEndpoint> { std::in_place, pAPI->bind, true };
};

HTTPEndpoint::HTTPEndpoint(TCPPeeraddr bind, bool isPublic)
    : bind(bind)
    , isPublic(isPublic)
    , app(lc.loop)
{
    spdlog::info("RPC {}endpoint is {}.", isPublic ? "public " : "", bind.to_string());
}


void HTTPEndpoint::shutdown()
{
    bshutdown = true;
    if (listen_socket != nullptr) {
        us_listen_socket_close(0, listen_socket);
        listen_socket = nullptr;
    }
}

void HTTPEndpoint::on_event(event_t&& e)
{
    auto handle_event { [&](const auto& event) {
        auto txt { nlohmann::json {
            { "type", event.eventName },
            { "data", jsonmsg::to_json(event) } }
                       .dump() };
        app.publish(event.eventName, txt, uWS::OpCode::TEXT);
    } };
    std::visit([&](auto&& e) {
        handle_event(std::move(e));
    },
        std::move(e));
}

void HTTPEndpoint::send_reply(uWS::HttpResponse<false>* res, const std::string& s)
{
    auto iter = pendingRequests.find(res);
    if (iter != pendingRequests.end()) {
        reply_json(res, s);
        pendingRequests.erase(iter);
    }
}

void HTTPEndpoint::on_aborted(uWS::HttpResponse<false>* res)
{
    pendingRequests.erase(res);
}

void HTTPEndpoint::on_listen(us_listen_socket_t* ls)
{
    listen_socket = ls;
    if (listen_socket) {
        if (bshutdown) {
            us_listen_socket_close(0, listen_socket);
        }
    } else
        throw std::runtime_error("Cannot listen on " + bind.to_string());
}
