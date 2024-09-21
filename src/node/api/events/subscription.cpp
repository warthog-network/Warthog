#include "subscription.hpp"
#include "api/http/json.hpp"
#include "api/interface.hpp"
#include "nlohmann/json.hpp"
#ifndef DISABLE_LIBUV
#include "api/http/endpoint.hpp"
#include "api/interface.hpp"
#include "global/globals.hpp"
#endif

using nlohmann::json;

namespace subscription {
void handleSubscriptioinMessage(const nlohmann::json& j, subscription_ptr p)
{
    auto action {
        [&j]() {
            auto s { j["action"].get<std::string>() };
            if (s == "subscribe")
                return subscription::Action::Subscribe;
            else if (s == "unsubscribe")
                return subscription::Action::Unsubscribe;
            throw std::runtime_error("Invalid subscribe action");
        }()
    };
    auto topic { j["topic"].get<std::string>() };
    if (topic == "connection") {
        subscribe_connection_event({ std::move(p), action });
    } else if (topic == "account") {
        subscribe_account_event({ std::move(p), action }, Address(j["params"]["address"].get<std::string>()));
    } else if (topic == "chain") {
        subscribe_chain_event({ std::move(p), action });
    } else {
        throw std::runtime_error("Invalid subscription topic");
    }
}

namespace events {
    namespace {
        json to_json(const AccountState a)
        {
            json arr((json::array_t()));
            Funds balance { [&]() {
                if (a.history) {
                    for (auto& b : std::ranges::reverse_view(a.history->blocks_reversed)) {
                        arr.push_back(jsonmsg::to_json(b));
                    }
                    return a.history->balance;
                }
                return Funds::zero();
            }() };
            return json {
                { "address", a.address.to_string() },
                { "balance", balance.to_string() },
                { "balanceE8", balance.E8() },
                { "history", arr },
            };
        }
        json to_json(const AccountDelta& a)
        {
            return json {
                { "address", a.address.to_string() },
                { "balance", a.newBalance.to_string() },
                { "balanceE8", a.newBalance.E8() },
                { "history", jsonmsg::to_json(a.newTransactions) },
            };
        };

        //////////////////////////////
        /// Connection events
        json to_json(const Connection& a)
        {
            return json {
                { "id", a.id },
                { "since", a.since },
                { "peerAddr", a.peerAddr },
                { "inbound", a.inbound }
            };
        };
        json to_json(const ConnectionsState& a)
        {
            auto arr(json::array());
            for (auto& c : a.connections) {
                arr.push_back(to_json(c));
            }
            return json {
                { "connections", arr },
                { "total",  },
            };
        };
        json to_json(const ConnectionsRemove& a)
        {
            return json {
                { "id", a.id },
                { "total", a.total },
            };
        };
        json to_json(const ConnectionsAdd a)
        {
            return json {
                { "connection", to_json(a.connection) },
                { "total", a.total }
            };
        };
        json to_json(const ChainState& a)
        {
            auto arr(json::array());
            for (auto& b : a.latestBlocks) {
                arr.push_back(jsonmsg::to_json(b));
            }
            return json {
                { "summary", jsonmsg::to_json(a.head) },
                { "blockHistory", arr },
            };
        };
        json to_json(const ChainAppend& a)
        {
            return json {
                { "head", jsonmsg::to_json(a.head) },
                { "newBlocks", jsonmsg::to_json(a.newBlocks) },
            };
        };
        json to_json(const ChainFork& a)
        {
            return json {
                { "head", jsonmsg::to_json(a.head) },
                { "latestBlocks", jsonmsg::to_json(a.latestBlocks) },
                { "rollbackLength", a.rollbackLength.value() },
            };
        };
        template <typename T>
        std::string json_str(T&& t)
        {
            auto j { to_json(t) };
            j["eventName"] = t.eventName;
            return j.dump();
        }
    }

    std::string Event::json_str() const
    {
        return std::visit([](auto&& e) {
            json j { to_json(e) };
            j["eventName"] = e.eventName;
            return j.dump();
        },
            variant);
    }

    void Event::send(subscription_ptr p) &&
    {
        std::move(*this).send(std::vector<subscription_ptr> { p });
    }

    void Event::send(std::vector<subscription_ptr> subscribers) &&
    {
#ifndef DISABLE_LIBUV
        global().httpEndpoint->send_event(std::move(subscribers), std::move(*this));
#endif
    }

}
// namespace topics {
//     namespace {
//         template <typename... T>
//         struct Parser;
//
//         struct ParseArg {
//             const std::string& topic;
//             bool subscribe;
//             const json& params;
//         };
//         template <>
//         struct Parser<> {
//             static Request::variant_t parse(const ParseArg&)
//             {
//                 throw std::runtime_error("Cannot parse json");
//             }
//         };
//
//         template <typename T>
//         struct Constructor;
//
//         template <typename T>
//         requires(std::is_constructible_v<T, RequestParams> == false)
//         struct Constructor<T> {
//             static Subscribe<T> construct(const json&) { return {}; }
//         };
//
//         template <typename T>
//         requires std::is_constructible_v<T, RequestParams>
//         struct Constructor<T> {
//             static Subscribe<T> construct(const json& j) { return { { j } }; }
//         };
//
//         template <typename T1, typename... T>
//         struct Parser<T1, T...> {
//             static Request::variant_t parse(const ParseArg& p)
//             {
//                 if (p.topic == T1::subscriptionTopic) {
//                     if (p.subscribe) {
//                         return Constructor<T1>::construct(p.params);
//                     }
//                     return Unsubscribe<T1>();
//                 }
//                 return Parser<T...>::parse(p);
//             }
//         };
//
//         template <typename T>
//         struct Unwrapper { };
//
//         template <typename... T>
//         struct Unwrapper<variant_builder<T...>> {
//             static auto parse(const ParseArg& p)
//             {
//                 return Parser<T...>::parse(p);
//             }
//         };
//     }
//
//     Account::Account(RequestParams p)
//         : address(p.j["address"].get<std::string>())
//     {
//     }
//
// }
// std::string get_string(const json& j, std::string_view key)
// {
//     return j[key].get<std::string>();
// }
// Request Request::parse(std::string_view sv)
// {
//     auto j { json::parse(sv) };
//     auto action { get_string(j, "action") };
//     bool subscribe {
//         [&action]() {
//             if (action == "subscribe")
//                 return true;
//             else if (action == "unsubscribe")
//                 return false;
//             throw std::runtime_error("Invalid subscribe action");
//         }()
//     };
//     auto topic { get_string(j, "topic") };
//     return topics::Unwrapper<topics::helper_t>::parse({ .topic { topic }, .subscribe = subscribe, .params { j["params"] } });
// }
}
