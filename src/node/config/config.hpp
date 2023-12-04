#pragma once

#include "block/chain/signed_snapshot.hpp"
#include "general/tcp_util.hpp"
#include <atomic>
struct gengetopt_args_info;
struct Config {
    struct Data {
        std::string chaindb;
        std::string peersdb;
    } data;
    struct JSONRPC {
        EndpointAddress bind;
    } jsonrpc;
    struct Node {
        std::optional<SnapshotSigner> snapshotSigner;
        EndpointAddress bind;
        std::atomic<bool> logCommunication { false };
    } node;
    struct Peers {
        bool allowLocalhostIp = false; // do not ignore 127.xxx.xxx.xxx peer node addresses provided by peers
        std::vector<EndpointAddress> connect;
        bool enableBan { false };
    } peers;
    bool localDebug { false };

    std::string dump();
    const std::string defaultDataDir;
    int init(int argc, char** argv);

    Config();
private:
    int process_gengetopt(gengetopt_args_info&);
};
