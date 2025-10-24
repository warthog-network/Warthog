#include "config.hpp"
#include "cmdline/cmdline.hpp"
#include "general/errors.hpp"
#include "general/is_testnet.hpp"
#include "general/tcp_util.hpp"
#include "spdlog/spdlog.h"
#include "toml++/toml.hpp"
#include "version.hpp"
#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#ifdef __APPLE__
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace std;

std::string get_default_datadir()
{
    const char* osBaseDir = nullptr;
#ifdef __linux__
    if ((osBaseDir = getenv("HOME")) == NULL) {
        osBaseDir = getpwuid(getuid())->pw_dir;
    }
    if (osBaseDir == nullptr)
        throw std::runtime_error("Cannot determine default data directory.");
    return std::string(osBaseDir) + "/.warthog/";
#elif _WIN32
    osBaseDir = getenv("LOCALAPPDATA");
    if (osBaseDir == nullptr)
        throw std::runtime_error("Cannot determine default data directory.");
    return std::string(osBaseDir) + "/Warthog/";
#elif __APPLE__
    if ((osBaseDir = getenv("HOME")) == NULL) {
        osBaseDir = getpwuid(getuid())->pw_dir;
    }
    if (osBaseDir == nullptr)
        throw std::runtime_error("Cannot determine default data directory.");
    return std::string(osBaseDir) + "/Library/Warthog/";
#else
    throw std::runtime_error("Cannot determine default data directory.");
#endif
}

Config::Config()
    : defaultDataDir(get_default_datadir())
{
}

namespace {
std::optional<SnapshotSigner> parse_leader_key(std::string privKey)
{
    try {
        SnapshotSigner ss { PrivKey(privKey) };
        spdlog::warn("This node signs chain snapshots with priority {}", ss.get_importance());
        return ss;
    } catch (Error e) {
        spdlog::warn("Cannot parse leader key, ignoring.");
    }
    return {};
}

}

int Config::init(int argc, char** argv)
{
    // default peer

    gengetopt_args_info ai;
    if (cmdline_parser(argc, argv, &ai) != 0) {
        return -1;
    }
    int res = process_gengetopt(ai);
    cmdline_parser_free(&ai);
    return res;
}
namespace {
void warning_config(const toml::key k)
{
    spdlog::warn("Ignoring configuration setting \""s + std::string(k) + "\" (line "s + std::to_string(k.source().begin.line) + ")");
}

template <typename T>
[[nodiscard]] auto fetch(toml::node& n)
{
    auto val = n.value<T>();
    if (val) {
        return val.value();
    }
    throw std::runtime_error("Cannot extract configuration value starting at line "s + std::to_string(n.source().begin.line) + ", colum "s + std::to_string(n.source().begin.column) + ".");
}

EndpointAddress fetch_endpointaddress(toml::node& n)
{
    auto p = EndpointAddress::parse(fetch<std::string>(n));
    if (p) {
        return p.value();
    }
    throw std::runtime_error("Cannot extract configuration value starting at line "s + std::to_string(n.source().begin.line) + ", colum "s + std::to_string(n.source().begin.column) + ".");
}
toml::array& array_ref(toml::node& n)
{
    if (n.is_array()) {
        return *n.as_array();
    }
    throw std::runtime_error("Expecting array at line "s + std::to_string(n.source().begin.line) + ".");
}
EndpointVector parse_endpoints(std::string csv)
{
    std::vector<EndpointAddress> out;
    std::string::size_type pos = 0;
    while (true) {
        auto end = csv.find(",", pos);
        auto param = csv.substr(pos, end - pos);
        auto parsed = EndpointAddress::parse(param);
        if (!parsed) {
            throw std::runtime_error("Invalid parameter '"s + param + "'."s);
        }
        out.push_back(parsed.value());
        if (end == std::string::npos) {
            break;
        }
        pos = end + 1;
    }
    return out;
}
} // namespace

int Config::process_gengetopt(gengetopt_args_info& ai)
{
    bool dmp(ai.dump_config_given);
    if (!dmp) {
        spdlog::info("Warthog Node v{}.{}.{} ", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
        std::cout << "                                          %%%                \n"
                     "                            .%%%%%%%%%%%%  %%                \n"
                     "                      %%%%%%%%%%%%%%%%%%%%%%%%               \n"
                     "                       %%%%%%%%%%%%%%%%%%%%%%%#%%            \n"
                     "                  =%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%           \n"
                     "                 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%          \n"
                     "            .%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%( )%%         \n"
                     "          .%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%       \n"
                     "        %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%    \n"
                     "      %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%@\n"
                     "     %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% \n"
                     "  .%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%=   *%%%%%%%   \n"
                     "  *    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%            \n"
                     "        %%%%%%%%%%%%%%%%%%%%%%%%%%%= %%%%%%%%%%%%%%%         \n"
                     "        %%%%%%%%%%%%%%%%%%%%%%%%   %%+       @%%%%%%%        \n"
                     "        %%%%%%%%%%%%%%%%%%%%       .%%%%%%%%.  %%%%%%%       \n"
                     "        %%%%%%%%%%%%%%%=                %%%%%%  %%%          \n"
                     "       %%%%%%%%%%%     @                 %%%%%               \n"
                     "      %%%%%%%%.   %%%%                                       \n"
                     "     %%%%%%%.  %%%%%%                                        \n"
                     "    %%%%%%.  %%%%%%                                          \n"
                     "   %%%%%.  %%%%%%%                                           \n\n";
    }

    // Log
    if (ai.debug_given)
        spdlog::set_level(spdlog::level::debug);

    if (!std::filesystem::exists(defaultDataDir)) {
        if (!dmp)
            spdlog::info("Crating default directory {}", defaultDataDir);
        std::error_code ec;
        if (!std::filesystem::create_directories(defaultDataDir, ec)) {
            throw std::runtime_error("Cannot create default directory " + defaultDataDir + ": " + ec.message());
        }
    }
    // copy default values
    std::optional<EndpointAddress> nodeBind;
    std::optional<EndpointAddress> rpcBind;
    std::optional<EndpointAddress> publicrpcBind;
    std::optional<EndpointAddress> stratumBind;
    node.isolated = ai.isolated_given;
    node.disableTxsMining = ai.disable_tx_mining_given;
    if (ai.testnet_given) {
        enable_testnet();
    }
    if (ai.enable_public_given) {
        publicrpcBind = EndpointAddress("0.0.0.0:3001");
    }

    if (is_testnet()) {
        peers.connect = {
            "193.218.118.57:9286",
            "98.71.18.140:9286"
        };
    } else {
        peers.connect = {
            "135.181.77.214:9186",
            "15.235.162.190:9186",
            "193.218.118.57:9186",
            "203.25.209.147:9186",
            "213.199.59.252:20016",
            "37.114.63.165:9186",
            "64.92.35.4:9186",
            "68.227.255.200:9186",
            "74.122.131.1:9186",
            "81.163.20.40:9186",
            "88.11.56.103:9186",
            "89.163.224.253:9186",
            "96.41.20.26:9186",
        };
    }

    std::string filename = is_testnet() ? "testnet_config.toml" : "config.toml";
    if (!ai.config_given && !std::filesystem::exists(filename)) {
        if (!dmp)
            spdlog::debug("No config.toml file found, using default configuration");
        if (ai.test_given) {
            spdlog::error("No configuration file found.");
            return -1;
        }
    } else {
        if (ai.config_given)
            filename = ai.config_arg;
        if (!dmp)
            spdlog::info("Reading configuration file \"{}\"", filename);
        try {
            // overwrite with config file
            toml::table tbl = toml::parse_file(filename);
            for (auto& [key, val] : tbl) {
                auto t = val.as_table();
                if (key == "db") {
                    for (auto& [k, v] : *t) {
                        if (k == "chain-db")
                            data.chaindb = fetch<std::string>(v);
                        else if (k == "peers-db")
                            data.peersdb = fetch<std::string>(v);
                        else
                            warning_config(k);
                    }
                } else if (key == "stratum") {
                    for (auto& [k, v] : *t) {
                        if (k == "bind")
                            stratumBind = fetch_endpointaddress(v);
                        else
                            warning_config(k);
                    }
                } else if (key == "publicrpc") {
                    for (auto& [k, v] : *t) {
                        if (k == "bind")
                            publicrpcBind = fetch_endpointaddress(v);
                        else
                            warning_config(k);
                    }
                } else if (key == "jsonrpc") {
                    for (auto& [k, v] : *t) {
                        if (k == "bind")
                            rpcBind = fetch_endpointaddress(v);
                        else
                            warning_config(k);
                    }
                } else if (key == "node") {
                    for (auto& [k, v] : *t) {
                        if (k == "bind") {
                            nodeBind = fetch_endpointaddress(v);
                        } else if (k == "connect") {
                            peers.connect.clear();
                            toml::array& c = array_ref(v);
                            for (auto& e : c) {
                                peers.connect.push_back(fetch_endpointaddress(e));
                            }
                        } else if (k == "leader-key") {
                            node.snapshotSigner = parse_leader_key(fetch<std::string>(v));
                        } else if (k == "isolated") {
                            node.isolated = fetch<bool>(v);
                        } else if (k == "disable-tx-mining") {
                            node.disableTxsMining = fetch<bool>(v);
                        } else if (k == "enable-ban") {
                            peers.enableBan = fetch<bool>(v);
                        } else if (k == "allow-localhost-ip") {
                            peers.allowLocalhostIp = fetch<bool>(v);
                        } else if (k == "log-communication") {
                            node.logCommunication = fetch<bool>(v);
                        } else
                            warning_config(k);
                    }
                } else {
                    warning_config(key);
                }
            }
            if (ai.test_given) {
                std::cout << "Configuration file \"" + filename + "\" is vaild.\n";
                return 0;
            }
        } catch (const toml::parse_error& err) {
            std::cerr << "Error while parsing file '" << *err.source().path << "':\n"
                      << err.description() << "\n  (" << err.source().begin
                      << ")\n";
            return -1;
        } catch (const std::runtime_error& e) {
            std::cerr << e.what();
            return -1;
        }
    }

    // DB args
    if (ai.chain_db_given)
        data.chaindb = ai.chain_db_arg;
    else {
        if (data.chaindb.empty())
            data.chaindb = defaultDataDir + (is_testnet() ? "testnet3_chain.db3" : "chain.db3");
    }
    if (ai.peers_db_given)
        data.peersdb = ai.peers_db_arg;
    else {
        if (data.peersdb.empty()) {
            data.peersdb = defaultDataDir + (is_testnet() ? "testnet_peers.db3" : "peers.db3");
        }
    }
    if (ai.temporary_given)
        data.chaindb = "";

    // Stratum API socket
    if (ai.stratum_given) {
        auto p = EndpointAddress::parse(ai.stratum_arg);
        if (!p) {
            std::cerr << "Bad --stratum option '" << ai.rpc_arg << "'.\n";
            return -1;
        };
        stratumPool = StratumPool { .bind = p.value() };
    } else {
        if (stratumBind) {
            stratumPool = StratumPool { *stratumBind };
        }
    }

    // JSON RPC socket
    if (ai.rpc_given) {
        auto p = EndpointAddress::parse(ai.rpc_arg);
        if (!p) {
            std::cerr << "Bad --rpc option '" << ai.rpc_arg << "'.\n";
            return -1;
        };
        jsonrpc.bind = p.value();
    } else {
        if (rpcBind) {
            jsonrpc.bind = *rpcBind;
        } else {
            if (is_testnet())
                jsonrpc.bind = EndpointAddress::parse("127.0.0.1:3100").value();
            else
                jsonrpc.bind = EndpointAddress::parse("127.0.0.1:3000").value();
        }
    }

    // JSON Public RPC socket
    if (ai.publicrpc_given) {
        auto p = EndpointAddress::parse(ai.publicrpc_arg);
        if (!p) {
            std::cerr << "Bad --publicrpc option '" << ai.rpc_arg << "'.\n";
            return -1;
        };
        publicAPI = PublicAPI { p.value() };
    } else {
        if (publicrpcBind) {
            publicAPI = PublicAPI(publicrpcBind.value());
        }
    }

    // Node socket
    if (ai.bind_given) {
        auto p = EndpointAddress::parse(ai.bind_arg);
        if (!p) {
            std::cerr << "Bad --bind option '" << ai.bind_arg << "'.\n";
            return -1;
        };
        node.bind = p.value();
    } else {
        if (nodeBind)
            node.bind = *nodeBind;
        else {
            if (is_testnet())
                node.bind = EndpointAddress::parse("0.0.0.0:9286").value();
            else
                node.bind = EndpointAddress::parse("0.0.0.0:9186").value();
        }
    }
    if (ai.minfee_given) {
        using namespace std::string_literals;

        try {
            node.minMempoolFee = CompactUInt::compact(Funds::parse_throw(ai.minfee_arg), true);
        } catch (...) {
            throw std::runtime_error("Can't parse minimal fee '"s + std::string(ai.minfee_arg) + "'."s);
        }
    }
    if (ai.connect_given) {
        peers.connect = parse_endpoints(ai.connect_arg);
    }

    if (dmp) {
        std::cout << dump();
        return 0;
    }
    return 1;
}

std::string Config::dump()
{
    toml::table tbl;
    tbl.insert_or_assign("jsonrpc", toml::table {
                                        { "bind", jsonrpc.bind.to_string() },
                                    });

    toml::array connect;
    for (auto ea : peers.connect) {
        connect.push_back(ea.to_string());
    }
    tbl.insert_or_assign("stratum",
        toml::table {
            { "bind", stratumPool ? stratumPool->bind.to_string() : ""s },
        });
    tbl.insert_or_assign("node",
        toml::table {
            { "bind", node.bind.to_string() },
            { "connect", connect },
            { "isolated", node.isolated },
            { "disable-tx-mining", node.disableTxsMining },
            { "enable-ban", peers.enableBan },
            { "allow-localhost-ip", peers.allowLocalhostIp },
            { "log-communication", (bool)node.logCommunication } });
    tbl.insert_or_assign("db", toml::table {
                                   { "chain-db", data.chaindb },
                                   { "peers-db", data.peersdb },
                               });
    stringstream ss;
    ss << tbl << endl;
    return ss.str();
}
