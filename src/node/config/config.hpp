#pragma once

#include "block/chain/signed_snapshot.hpp"
#include "expected.hpp"
#include "transport/helpers/peer_addr.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
#include "transport/helpers/transport_types.hpp"
#include "types.hpp"
#include <atomic>
struct gengetopt_args_info;
struct EndpointVector : public std::vector<TCPPeeraddr> {
    EndpointVector() { }
    EndpointVector(std::vector<TCPPeeraddr> v)
        : vector(std::move(v))
    {
    }
    EndpointVector(std::initializer_list<std::string> l)
    {
        for (auto& s : l) {
            push_back(TCPPeeraddr { s });
        }
    }
};
struct ConfigParams {
    static constexpr IPv4 localhost { "127.0.0.1" };
    static bool mount_opfs(const char* mountpoint);
    struct AllowedInboundTransports {
        struct V4V6 {
            const bool v4 { true };
            const bool v6 { true };
            bool allowed(IP ip) const
            {
                if (ip.is_v4())
                    return v4;
                return v6;
            }
        };
        bool allowed(IP ip, TransportType tt) const
        {
            using enum TransportType;
            switch (tt) {
            case TCP:
                return tcp.allowed(ip);
            case Websocket:
                return websocket.allowed(ip);
            case WebRTC:
                return webRTC.allowed(ip);
            }
        }
        V4V6 tcp { true, false };
        V4V6 websocket { true, true };
        V4V6 webRTC { true, false };
    } allowedInboundTransports;
    struct Data {
        std::string chaindb;
        std::string peersdb;
    } data;
    struct JSONRPC {
        TCPPeeraddr bind { localhost, 3000 };
    } jsonrpc;
    struct PublicAPI {
        TCPPeeraddr bind;
    };
    struct StratumPool {
        TCPPeeraddr bind;
    };
    std::optional<PublicAPI> publicAPI;
    std::optional<StratumPool> stratumPool;
    WebsocketServerConfig websocketServer;
    struct Node {
        static constexpr TCPPeeraddr default_endpoint { localhost, DEFAULT_ENDPOINT_PORT };
        std::optional<SnapshotSigner> snapshotSigner;
        TCPPeeraddr bind { default_endpoint };
        bool isolated { false };
        bool disableTxsMining { false }; // don't mine transactions
        bool logCommunicationVal { false };
    } node;
    struct Peers {
        bool allowLocalhostIp = false; // do not ignore 127.xxx.xxx.xxx peer node addresses provided by peers
#ifndef DISABLE_LIBUV
        EndpointVector connect;
#else
        std::vector<WSUrladdr> connect;
#endif
        bool enableBan { true };
    } peers;
    bool localDebug { false };

    static std::string get_default_datadir();
    std::string dump();
    [[nodiscard]] static tl::expected<ConfigParams, int> from_args(int argc, char** argv);

private:
    int init(const gengetopt_args_info&);
};
struct Config : public ConfigParams {
    Config(ConfigParams&&);
    std::atomic<bool> logCommunication { false };
};
