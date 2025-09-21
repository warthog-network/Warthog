#pragma once
#include "api/http/json.hpp"
// #include "general/funds.hpp"
#include "api/http/parse.hpp"
#include "api/types/accountid_or_address.hpp"
#include "api/types/all.hpp"
#include "api/types/input.hpp"
#include "chainserver/transaction_ids.hpp"
#include "communication/mining_task.hpp"
#include "communication/rxtx_server/rxtx_server.hpp"
#include "general/hex.hpp"
#include "http/json.hpp"
#include "spdlog/spdlog.h"
#include <charconv>
#include <string>

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
            throw Error(EINV_ARGS);
        }
        return res;
    }
    operator api::HeightOrHash()
    {
        if (sv.length() == 64)
            return { Hash { *this } };
        return { Height { *this } };
    }
    operator api::AccountIdOrAddress()
    {
        if (sv.length() == 48)
            return { Address { *this } };
        return { AccountId { static_cast<uint64_t>(*this) } };
    }
    operator PrivKey()
    {
        return PrivKey(sv);
    }
    operator api::AssetIdOrHash()
    {
        if (sv.length() == 64)
            return { AssetHash(*this) };
        return { AssetId(*this) };
    }
    operator api::TokenIdOrSpec()
    {
        if (sv.length() >= 64)
            return { api::TokenSpec::parse_throw(*this) };
        return { TokenId(*this) };
    }
    operator ParsedFunds()
    {
        return ParsedFunds(sv);
    }
    operator Wart()
    {
        return Wart::parse_throw(sv);
    }
    operator Page()
    {
        return static_cast<uint32_t>(*this);
    }
    operator Hash()
    {
        return hex_to_arr<32>(sv);
    }
    operator TxHash()
    {
        return TxHash(static_cast<Hash>(*this));
    }
    operator NonzeroHeight()
    {
        return Height(static_cast<uint32_t>(*this)).nonzero_throw(EBADHEIGHT);
    }
    operator Height()
    {
        return Height(static_cast<uint32_t>(*this));
    }
    operator std::string_view()
    {
        return sv;
    }
    operator Address()
    {
        return Address(sv);
    }
};
}
template <typename T>
class RouterHook {
public:
    static void hook_get(T& t, std::string pattern, auto asyncfun, auto serializer, bool priv = false)
    {
        if (priv && t.isPublic)
            return;
        t.indexGenerator.get(pattern);
        t.router().get(pattern,
            [&t, asyncfun, serializer](auto* res, auto* req) {
                spdlog::debug("GET {}", req->getUrl());
                asyncfun(
                    [&t, res, serializer](auto& data) {
                        t.async_reply(res, serializer(data));
                    });
                t.insert_pending(res);
            });
    }
    static void hook_get(T& t, std::string pattern, auto asyncfun, bool priv = false)
    {
        if (priv && t.isPublic)
            return;
        t.indexGenerator.get(pattern);
        t.router().get(pattern,
            [&t, asyncfun](auto* res, auto* req) {
                spdlog::debug("GET {}", req->getUrl());
                asyncfun(
                    [&t, res]<typename R>(R&& data) {
                        t.async_reply(res, jsonmsg::serialize(std::forward<R>(data)));
                    });
                t.insert_pending(res);
            });
    }

    static void hook_get_1(T& t, std::string pattern, auto asyncfun, bool priv = false)
    {
        if (priv && t.isPublic)
            return;
        t.indexGenerator.get(pattern);
        t.router().get(pattern,
            [&t, asyncfun](auto* res, auto* req) {
                spdlog::debug("GET {}", req->getUrl());
                try {
                    ParameterParser p1 { req->getParameter(0) };
                    asyncfun(p1,
                        [&t, res](auto& data) {
                            t.async_reply(res, jsonmsg::serialize(data));
                        });
                    t.insert_pending(res);
                } catch (Error e) {
                    t.reply_json(res, jsonmsg::serialize(tl::make_unexpected(e)));
                }
            });
    }
    static void hook_get_2(T& t, std::string pattern, auto asyncfun, bool priv = false)
    {
        if (priv && t.isPublic)
            return;
        t.indexGenerator.get(pattern);
        t.router().get(pattern,
            [&t, asyncfun](auto* res, auto* req) {
                spdlog::debug("GET {}", req->getUrl());
                try {
                    ParameterParser p1 { req->getParameter(0) };
                    ParameterParser p2 { req->getParameter(1) };
                    asyncfun(p1, p2,
                        [&t, res](auto& data) {
                            t.async_reply(res, jsonmsg::serialize(data));
                        });
                    t.insert_pending(res);
                } catch (Error e) {
                    t.reply_json(res, jsonmsg::serialize(tl::make_unexpected(e)));
                }
            });
    }
    static void hook_get_3(T& t, std::string pattern, auto asyncfun, bool priv = false)
    {
        if (priv && t.isPublic)
            return;
        t.indexGenerator.get(pattern);
        t.router().get(pattern,
            [&t, asyncfun](auto* res, auto* req) {
                spdlog::debug("GET {}", req->getUrl());
                try {
                    ParameterParser p1 { req->getParameter(0) };
                    ParameterParser p2 { req->getParameter(1) };
                    ParameterParser p3 { req->getParameter(2) };
                    asyncfun(p1, p2, p3,
                        [&t, res](auto& data) {
                            t.async_reply(res, jsonmsg::serialize(data));
                        });
                    t.insert_pending(res);
                } catch (Error e) {
                    t.reply_json(res, jsonmsg::serialize(tl::make_unexpected(e)));
                }
            });
    }

    static void hook_post(T& t, std::string pattern, auto parser, auto asyncfun, bool priv = false)
    {
        if (priv && t.isPublic)
            return;
        t.indexGenerator.post(pattern);
        t.router().post(pattern,
            [&t, parser = std::move(parser), asyncfun = std::move(asyncfun)](auto* res, auto* req) {
                spdlog::debug("POST {}", req->getUrl());
                std::vector<uint8_t> body;

                res->onData(
                    [&t, asyncfun = std::move(asyncfun), parser = std::move(parser), res, body = std::move(body)](std::string_view data, bool last) mutable {
                        body.insert(body.end(), data.begin(), data.end());
                        if (last) {
                            try {
                                asyncfun(parser(body),
                                    [&t, res](auto& data) {
                                        t.async_reply(res, jsonmsg::serialize(data));
                                    });
                            } catch (Error e) {
                                auto ser = jsonmsg::serialize(tl::make_unexpected(e));
                                t.async_reply(res, ser);
                            }
                        }
                    });
                t.insert_pending(res);
            });
    }
    static void hook_endpoints(T& t)
    {
        t.indexGenerator.section("Transaction Endpoints");
        hook_post(t, "/transaction/add", parse_payment_create, put_mempool);
        hook_get(t, "/transaction/mempool", get_mempool);
        hook_get_1(t, "/transaction/lookup/:txid", lookup_tx);
        hook_get(t, "/transaction/latest", get_latest_transactions);

        t.indexGenerator.section("Chain Endpoints");
        hook_get(t, "/chain/head", get_block_head);
        hook_get(t, "/chain/grid", get_chain_grid, true);
        hook_get_1(t, "/chain/block/:id/hash", get_chain_hash);
        hook_get_1(t, "/chain/block/:id/header", get_chain_header);
        hook_get_1(t, "/chain/block/:id", get_chain_block);
        hook_get_1(t, "/chain/mine/:account", get_chain_mine);
        hook_get_1(t, "/chain/mine/:account/log", get_chain_mine);
        hook_get(t, "/chain/signed_snapshot", get_signed_snapshot, true);
        hook_get(t, "/chain/txcache", get_txcache);
        hook_get_1(t, "/chain/hashrate/:window", get_hashrate_n);
        hook_get_3(t, "/chain/hashrate/chart/block/:from/:to/:window", get_hashrate_block_chart, true);
        hook_get_3(t, "/chain/hashrate/chart/time/:from/:to/:interval", get_hashrate_time_chart, true);
        hook_post(t, "/chain/append", parse_block_worker, put_chain_append, true);

        t.indexGenerator.section("Account Endpoints");
        hook_get_2(t, "/account/:account/balance/:token", get_account_token_balance);
        hook_get_2(t, "/account/:account/history/:beforeTxIndex", get_account_history);
        hook_get_1(t, "/account/richlist/:token", get_account_richlist);

        t.indexGenerator.section("Peers Endpoints");
        hook_get(t, "/peers/ip_count", get_ip_count);
        hook_get(t, "/peers/banned", get_banned_peers);
        hook_get(t, "/peers/unban", unban_peers, true);
        hook_get_1(t, "/peers/offenses/:page", get_offenses);
        hook_get(t, "/peers/connected", get_connected_peers2, true);
        hook_get_1(t, "/peers/disconnect/:id", disconnect_peer, true);
        hook_get(t, "/peers/throttled", get_throttled_peers, true);
        hook_get(t, "/peers/connected/connection", get_connected_connection);
        hook_get(t, "/peers/connection_schedule", get_connection_schedule);
        hook_get(t, "/peers/transmission_hours", get_transmission_hours, true);
        hook_get(t, "/peers/transmission_minutes", get_transmission_minutes, true);
        // hook_get(t,"/peers/endpoints", inspect_eventloop, jsonmsg::endpoints, true);
        // hook_get(t,"/peers/connect_timers", inspect_eventloop, jsonmsg::connect_timers, true);

        t.indexGenerator.section("Tools Endpoints");
        hook_get_1(t, "/tools/encode16bit/from_e8/:feeE8", get_round16bit_e8);
        hook_get_1(t, "/tools/encode16bit/from_string/:string", get_round16bit_funds);
        hook_get(t, "/tools/version", get_version);
        hook_get(t, "/tools/info", get_info);
        hook_get(t, "/tools/wallet/new", get_wallet_new);
        hook_get_1(t, "/tools/wallet/from_privkey/:privkey", get_wallet_from_privkey);
        hook_get_1(t, "/tools/janushash_number/:headerhex", get_janushash_number);
        hook_get_1(t, "/tools/sample_verified_peers/:number", sample_verified_peers);

        t.indexGenerator.section("Debug Endpoints");
        hook_get(t, "/debug/header_download", inspect_eventloop, jsonmsg::header_download, true);
        hook_get_1(t, "/loadtest/block_request/:conn_id", loadtest_block);
        hook_get_1(t, "/loadtest/header_request/:conn_id", loadtest_header);
        hook_get_1(t, "/loadtest/disable/:conn_id", loadtest_disable);
    }
};

template <typename T>
void hook_endpoints(T&& t)
{
    RouterHook<std::remove_reference_t<T>>::hook_endpoints(std::forward<T>(t));
}
