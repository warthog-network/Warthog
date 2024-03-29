#include "stratum_server.hpp"

#include "api/interface.hpp"
#include "block/header/header_impl.hpp"
#include "general/tcp_util.hpp"
#include "nlohmann/json.hpp"
#include <cassert>
#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <span>
#include <uvw.hpp>
#include <variant>

using namespace std;

namespace stratum {
namespace messages {
    struct MiningSubscribe {
        int64_t id;
    };
    struct MiningAuthorize {
        MiningAuthorize(int64_t id, nlohmann::json::array_t params)
            : id(id)
            , user(params[0].get<std::string>())
        {
        }
        int64_t id;
        std::string user;
    };
    struct MiningSubmit {
        int64_t id;
        MiningSubmit(int64_t id, nlohmann::json::array_t params)
            : id(id)
            , jobId(params[0].get<std::string>())
            , extranonce2(hex_to_arr<10>(params[1].get<std::string>()))
            , ntime(hex_to_arr<4>(params[2].get<std::string>()))
            , nonce(hex_to_arr<4>(params[3].get<std::string>()))
        {
        }
        void apply_to(Block& b) const
        {
            std::copy(extranonce2.begin(), extranonce2.end(), b.body.data().begin());
            b.header.set_merkleroot(b.body_view().merkle_root(b.height));
            b.header.set_nonce(nonce);
            b.header.set_timestamp(ntime);
        }
        std::string jobId;
        std::array<uint8_t, 10> extranonce2;
        std::array<uint8_t, 4> ntime;
        std::array<uint8_t, 4> nonce;
    };

    template <typename... T>
    std::string pool_null_message(std::string method, T... t)
    {
        return nlohmann::json {
            { "id", nullptr },
            { "method", method },
            { "params", nlohmann::json::array({ std::forward<T>(t)... }) }
        }.dump();
    }

    struct MiningNotify {
        std::string jobId;
        Hash prevHash;
        std::vector<uint8_t> merklePrefix;
        uint32_t version;
        uint32_t nbits;
        uint32_t ntime;
        bool clean { false };
        MiningNotify(std::string jobId, const Block& b, bool clean)
            : jobId(std::move(jobId))
            , prevHash { b.header.prevhash() }
            , merklePrefix(b.body_view().merkle_prefix())
            , version(b.header.version())
            , nbits(hton32(b.header.target_v2().binary()))
            , ntime(b.header.timestamp())
            , clean(clean)
        {
        }
        std::string to_string()
        {
            return pool_null_message("mining.notify",
                jobId,
                serialize_hex(prevHash),
                serialize_hex(merklePrefix),
                serialize_hex(version),
                serialize_hex(nbits),
                serialize_hex(ntime),
                clean);
        }
    };

    //     {
    //   "id": null,
    //   "method": "mining.notify",
    //   "params": [
    //     "jobId",         # jobId has to be sent back on submission
    //     "prevHash",      # hex encoded previous hash in block header
    //     "merklePrefix",  # hex encoded prefix of
    //     "version",       # hex encoded block version
    //     "nbits",         # hex encoded nbits, difficulty target
    //     "ntime",         # hex encoded ntime, timestamp in block header
    //     false            # clean? If yes, discard old jobs.
    //   ]
    // }

    struct MiningSetDifficulty {
        double difficulty;
        MiningSetDifficulty(const Block& b)
        {
            difficulty = b.header.target_v2().difficulty();
        }
        std::string to_string()
        {
            return pool_null_message("mining.set_difficulty", difficulty);
        }
    };
    struct OK {
        int64_t id;
        nlohmann::json result = true;
        OK(int64_t id, nlohmann::json result = true)
            : id(id)
            , result(std::move(result))
        {
        }
        std::string to_string()
        {
            return nlohmann::json {
                { "id", id },
                { "result", result },
                { "error", nullptr }
            }.dump();
        }
    };

    struct StratumError {
        int64_t id;
        int64_t code;
        std::string message;

        StratumError(int64_t id, int64_t code, std::string message)
            : id(id)
            , code(code)
            , message(std::move(message))
        {
        }
        std::string to_string()
        {
            return nlohmann::json {
                { "id", id },
                { "result", nullptr },
                { "error", nlohmann::json::array({ code, message, nullptr }) }
            }.dump();
        }
        static StratumError BadAddress(int64_t id) { return { id, 30, "User format must be <Address>[.<Workername>]"s }; }
        static StratumError Unauthorized(int64_t id) { return { id, 24, "Unauthorized worker."s }; }
        static StratumError JobNotFound(int64_t id) { return { id, 21, "Job not found"s }; }
    };

    OK SubscribeResponse(int64_t id)
    {
        return { id, nlohmann::json::array({ nlohmann::json::array({ "mining.notify", "" }), "", 10 }) };
    };

    using message = std::variant<MiningSubscribe, MiningAuthorize, MiningSubmit>;

    std::optional<messages::message> parse(std::string_view v)
    {
        using namespace nlohmann;
        using array_t = json::array_t;
        auto parsed = json::parse(v);
        auto method { parsed["method"].get<std::string>() };
        auto params(parsed["params"].get<array_t>());
        auto id { parsed["id"].get<int64_t>() };

        if (method == "mining.submit") {
            return MiningSubmit(id, params);
        } else if (method == "mining.subscribe") {
            return MiningSubscribe { .id = id };
        } else if (method == "mining.authorize") {
            return MiningAuthorize(id, params);
        }
        cout << "Cannot parse \"" << v << "\"" << endl;
        return {};
    }
}

void Connection::on_message(std::string_view msg)
{
    size_t lower = 0;
    for (size_t i = 0; i < msg.size(); ++i) {
        if (msg[i] == '\n') {
            stratumLine += msg.substr(lower, i - lower);
            lower = i + 1;
            process_line();
        }
    }
    stratumLine += msg.substr(lower, msg.size() - lower);
};

void Connection::on_append_result(int64_t stratumId, tl::expected<void, int32_t> result)
{
    if (result.has_value()) {
        write() << messages::OK(stratumId);
    } else {
        write() << messages::StratumError(stratumId, 40, Error(result.error()).strerror());
    }
};
using namespace stratum::messages;

void Connection::handle_message(messages::MiningSubscribe&& s)
{
    write() << SubscribeResponse(s.id);
}

void Connection::handle_message(messages::MiningSubmit&& m)
{
    if (!authorized) {
        write() << StratumError::Unauthorized(m.id);
        shutdown();
        return;
    }
    auto b { server.get_block(authorized->address, m.jobId) };
    if (!b) {
        write() << StratumError::JobNotFound(m.id);
        return;
    }
    m.apply_to(*b);
    put_chain_append({ *b },
        [&, p = shared_from_this(), id = m.id](const tl::expected<void, int32_t>& res) {
            server.on_append_result({ .p = p, .stratumId = id, .result { res } });
        });
}

void Connection::send_work(std::string jobId, const Block& block, bool clean)
{
    write() << MiningNotify(jobId, block, clean || fresh)
            << MiningSetDifficulty(block);
    fresh = false;
}

Connection::~Connection()
{
    if (authorized) {
        server.unlink_authorized(authorized->address, this);
    }
}

void Connection::handle_message(messages::MiningAuthorize&& m)
{
    auto pos = m.user.find(".");
    try {
        auto addrStr { m.user.substr(0, pos) };
        auto workerStr { m.user.substr(pos + 1) };
        Address addr(addrStr);
        authorized = Authorized { addr, workerStr };
        server.link_authorized(addr, this);
        write() << OK(m.id);
    } catch (Error& e) {
        write() << StratumError::BadAddress(m.id);
        shutdown();
    }
}

void send_work(std::string jobId, const Block& block);

void Connection::shutdown()
{
    handle->shutdown();
}

void Connection::write_line(const std::string& line)
{
    const size_t n { line.size() + 1 };
    auto p { std::make_unique<char[]>(n) };
    memcpy(p.get(), line.data(), line.size());
    p.get()[n - 1] = '\n';
    if (!handle->closing())
        handle->write(std::move(p), n);
}

void Connection::process_line()
{
    auto parsed = messages::parse(stratumLine);
    if (parsed) {
        std::visit([this](auto&& message) {
            handle_message(std::move(message));
        },
            *parsed);
    }
    stratumLine.clear();
}
}

StratumServer::AddressData::AddressData(std::function<Subscription()> generator)
    : subscription(generator()) {};

void StratumServer::handle_event(SubscriptionFeed&& fe)
{
    auto iter = addressData.find(fe.address);
    if (iter == addressData.end())
        return;
    auto& ad { iter->second };

    // register block
    auto jobId { serialize_hex(fe.t.block.header.hash()) };
    Block* b { ad.add_block(jobId, std::move(fe.t.block)) };
    if (b == nullptr)
        return;
    const auto& block { *b };

    // dispatch block
    for (auto* c : ad.connections) {
        c->send_work(jobId, block, ad.clean);
    }
}

void StratumServer::handle_event(ShutdownEvent&&)
{
    loop->walk([](auto&& h) { h.close(); });
}

void StratumServer::handle_event(AppendResult&& ar)
{
    auto p { ar.p.lock() };
    p->on_append_result(ar.stratumId, ar.result);
}

void StratumServer::handle_events()
{
    std::vector<Event> tmp;
    {
        std::lock_guard l(m);
        std::swap(tmp, events);
    }
    for (auto& e : tmp) {
        std::visit([&](auto&& e) {
            handle_event(std::move(e));
        },
            std::move(e));
    }
}

void StratumServer::acceptor(EndpointAddress endpointAddress)
{
    std::shared_ptr<uvw::tcp_handle> tcp = loop->resource<uvw::tcp_handle>();

    tcp->on<uvw::error_event>([](const uvw::error_event&, uvw::tcp_handle&) { /* something went wrong */ });
    tcp->on<uvw::listen_event>([this](const uvw::listen_event&, uvw::tcp_handle& srv) {
        std::shared_ptr<uvw::tcp_handle> client = srv.parent().resource<uvw::tcp_handle>();

        assert(srv.accept(*client) == 0);
        assert(client->read() == 0);
        connections.emplace_front(std::make_shared<stratum::Connection>(client, *this));
        auto iter = connections.begin();
        auto& con { **iter };
        client->on<uvw::close_event>([this, iter](const uvw::close_event&, uvw::tcp_handle&) {
            connections.erase(iter);
        });
        client->on<uvw::end_event>([](const uvw::end_event&, uvw::tcp_handle& client) { client.close(); });
        client->on<uvw::error_event>([](const uvw::error_event&, uvw::tcp_handle& client) { client.close(); });
        client->on<uvw::shutdown_event>([](const uvw::shutdown_event&, uvw::tcp_handle& client) { client.close(); });
        client->on<uvw::data_event>([&con](const uvw::data_event& de, uvw::tcp_handle& client) {
            try {
                con.on_message({ de.data.get(), de.length });
            } catch (const std::exception& e) {
                cout << "Error, closing connection: " << e.what() << endl;
                client.close();
            } catch (Error e) {
                cout << "Error, closing connection: " << e.strerror() << endl;
                client.close();
            }
        });
    });
    auto check_result = [](int ec) {
        if (ec != 0) {
            throw std::runtime_error(uv_strerror(ec));
        }
    };

    ;
    check_result(tcp->bind(endpointAddress.ipv4.to_string(), endpointAddress.port));
    check_result(tcp->listen());
}

StratumServer::StratumServer(EndpointAddress endpointAddress)
    : loop(uvw::loop::create())
    , async(loop->resource<uvw::async_handle>())
{
    spdlog::info("Starting Stratum server on {}", endpointAddress.to_string());
    async->on<uvw::async_event>([&](uvw::async_event&, uvw::async_handle&) {
        handle_events();
    });
    acceptor(endpointAddress);
    t = std::thread([&]() { loop->run(); });
}

StratumServer::~StratumServer()
{
    shutdown();
    t.join();
}

void StratumServer::push(Event e)
{
    std::lock_guard l(m);
    events.push_back(std::move(e));
    async->send();
}

Block* StratumServer::AddressData::find_block(const std::string& jobId)
{
    auto iter { blocks.find(jobId) };
    if (iter == blocks.end())
        return nullptr;
    return &iter->second;
}
Block* StratumServer::AddressData::add_block(const std::string& jobId, Block&& b)
{
    // delete old blocks when new block is available
    if (!blocks.empty() && blocks.begin()->second.header.prevhash() != b.header.prevhash()) {
        blocks.clear();
    }

    auto [b_iter, inserted] { blocks.try_emplace(jobId, std::move(b)) };
    if (!inserted)
        return nullptr;
    return &b_iter->second;
}

std::optional<Block> StratumServer::get_block(Address a, std::string jobId)
{
    auto iter = addressData.find(a);
    assert(iter != addressData.end());
    if (auto b { iter->second.find_block(jobId) }; b != nullptr) {
        return *b;
    }
    return {};
}

void StratumServer::shutdown()
{
    push(ShutdownEvent {});
}
void StratumServer::on_mining_task(Address a, MiningTask&& mt)
{
    push(SubscriptionFeed { a, std::move(mt) });
}

void StratumServer::on_append_result(AppendResult ar)
{
    push(std::move(ar));
}

void StratumServer::link_authorized(const Address& a, stratum::Connection* c)
{
    auto [iter, _] { addressData.try_emplace(a,
        [&]() -> mining_subscription::MiningSubscription {
            return subscribe_chain_mine(a,
                [a, this](MiningTask&& t) {
                    return on_mining_task(a, std::move(t));
                });
        }) };
    iter->second.connections.push_back(c);
}

void StratumServer::unlink_authorized(const Address& a, stratum::Connection* c)
{
    auto iter = addressData.find(a);
    assert(iter != addressData.end());
    auto& addrConnections { iter->second.connections };
    assert(std::erase(addrConnections, c) == 1);
    if (addrConnections.size() == 0) {
        addressData.erase(iter);
    }
}
