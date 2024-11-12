#pragma once

#include "block/chain/signed_snapshot.hpp"
#include "expected.hpp"
#include "transport/helpers/peer_addr.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
#include "transport/helpers/transport_types.hpp"
#include "types.hpp"
#include <atomic>
struct gengetopt_args_info;
struct Endpoints : public std::vector<TCPPeeraddr> {
    Endpoints() { }
    Endpoints(std::vector<TCPPeeraddr> v)
        : vector(std::move(v))
    {
    }
    Endpoints(std::initializer_list<std::string> l)
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
            return false; // should not happen but gcc complains about "warning: control reaches end of non-void function" without this statement
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
    std::optional<TCPPeeraddr> publicAPI;
    std::optional<TCPPeeraddr> stratumPool;
    WebsocketServerConfig websocketServer;
    struct Node {
        static constexpr TCPPeeraddr default_endpoint { localhost, DEFAULT_ENDPOINT_PORT };
        std::optional<SnapshotSigner> snapshotSigner;
        TCPPeeraddr bind { default_endpoint };
        bool isolated { false };
        bool disableTxsMining { false }; // don't mine transactions
        bool logCommunicationVal { false };
        bool logRTC { false };
    } node;
    struct Peers {
        bool allowLocalhostIp = false; // do not ignore 127.xxx.xxx.xxx peer node addresses provided by peers
#ifndef DISABLE_LIBUV
        Endpoints connect;
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
    ConfigParams() {};
    void prepare_warthog_dir(const std::string&, bool log);
    void assign_defaults();
    int init(const gengetopt_args_info&);
    void process_args(const gengetopt_args_info& ai);
    std::optional<int> process_config_file(const gengetopt_args_info& ai, bool silent);
};

struct Config : public ConfigParams {
    Config(ConfigParams&&);
    std::atomic<bool> logCommunication { false };
    std::atomic<bool> logRTC { false };
};
