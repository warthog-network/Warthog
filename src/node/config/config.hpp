#pragma once

#include "block/chain/signed_snapshot.hpp"
#include "types.hpp"
#include "expected.hpp"
#include "transport/helpers/socket_addr.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
#include <atomic>
struct gengetopt_args_info;
struct EndpointVector : public std::vector<TCPSockaddr> {
    EndpointVector(){}
    EndpointVector(std::vector<TCPSockaddr> v)
        : vector(std::move(v))
    {
    }
    EndpointVector(std::initializer_list<std::string> l)
    {
        for (auto& s : l) {
            push_back(TCPSockaddr { s } );
        }
    }
};
struct ConfigParams {
    static constexpr IPv4 localhost { "127.0.0.1" };
    static bool mount_opfs(const char* mountpoint);
    struct Data {
        std::string chaindb;
        std::string peersdb;
    } data;
    struct JSONRPC {
        TCPSockaddr bind { localhost, 3000 };
    } jsonrpc;
    struct PublicAPI {
        TCPSockaddr bind;
    };
    struct StratumPool {
        TCPSockaddr bind;
    };
    std::optional<PublicAPI> publicAPI;
    std::optional<StratumPool> stratumPool;
    WebsocketServerConfig websocketServer; 
    struct Node {
        static constexpr TCPSockaddr default_endpoint { localhost, DEFAULT_ENDPOINT_PORT };
        std::optional<SnapshotSigner> snapshotSigner;
        TCPSockaddr bind { default_endpoint };
        bool isolated { false };
        bool logCommunicationVal { false };
    } node;
    struct Peers {
        bool allowLocalhostIp = false; // do not ignore 127.xxx.xxx.xxx peer node addresses provided by peers
#ifndef DISABLE_LIBUV
        EndpointVector connect;
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
