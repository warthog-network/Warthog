#pragma once

#include "block/chain/signed_snapshot.hpp"
#include "expected.hpp"
#include "general/tcp_util.hpp"
#include <atomic>
struct gengetopt_args_info;
struct EndpointVector : public std::vector<EndpointAddress> {
    using vector::vector;
    EndpointVector(std::vector<EndpointAddress> v)
        : vector(std::move(v))
    {
    }
    EndpointVector(std::initializer_list<std::string> l)
    {
        for (auto& s : l) {
            push_back({ s });
        }
    }
};
struct ConfigParams {
    static constexpr IPv4 localhost { "127.0.0.1" };
    struct Data {
        std::string chaindb;
        std::string peersdb;
    } data;
    struct JSONRPC {
        EndpointAddress bind { localhost, 3000 };
    } jsonrpc;
    struct PublicAPI {
        EndpointAddress bind;
    };
    struct StratumPool {
        EndpointAddress bind;
    };
    std::optional<PublicAPI> publicAPI;
    std::optional<StratumPool> stratumPool;
    struct Node {
        static constexpr EndpointAddress default_endpoint { localhost, DEFAULT_ENDPOINT_PORT };
        std::optional<SnapshotSigner> snapshotSigner;
        EndpointAddress bind { default_endpoint };
        bool isolated { false };
        bool logCommunicationVal { false };
    } node;
    struct Peers {
        bool allowLocalhostIp = false; // do not ignore 127.xxx.xxx.xxx peer node addresses provided by peers
        EndpointVector connect;
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
