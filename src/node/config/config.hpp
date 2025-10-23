#pragma once

#include "block/chain/signed_snapshot.hpp"
#include "general/compact_uint.hpp"
#include "general/tcp_util.hpp"
#include <atomic>
struct gengetopt_args_info;
struct EndpointVector: public std::vector<EndpointAddress> {
    using vector::vector;
    EndpointVector(std::vector<EndpointAddress> v):vector(std::move(v)){}
    EndpointVector(std::initializer_list<std::string> l){
        for (auto &s : l) {
            push_back({s});
        }
    }
};
struct Config {
    struct Data {
        std::string chaindb;
        std::string peersdb;
    } data;
    struct JSONRPC {
        EndpointAddress bind;
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
        std::optional<SnapshotSigner> snapshotSigner;
        EndpointAddress bind;
        std::atomic<CompactUInt> minMempoolFee { CompactUInt::compact(Funds::from_value(9992).value()) };
        bool isolated { false };
        bool disableTxsMining { false }; // don't mine transactions
        std::atomic<bool> logCommunication { false };
    } node;
    struct Peers {
        bool allowLocalhostIp = false; // do not ignore 127.xxx.xxx.xxx peer node addresses provided by peers
        EndpointVector connect;
        bool enableBan { true };
    } peers;
    bool localDebug { false };

    std::string dump();
    const std::string defaultDataDir;
    int init(int argc, char** argv);

    Config();
private:
    int process_gengetopt(gengetopt_args_info&);
};
