#include "config.hpp"
#include "cmdline/cmdline.hpp"
#include "general/errors.hpp"
#include "general/is_testnet.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
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

std::string ConfigParams::get_default_datadir()
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

struct CmdlineParsed {
    static std::optional<CmdlineParsed> parse(int argc, char** argv)
    {
        gengetopt_args_info ai;
        if (cmdline_parser(argc, argv, &ai) != 0)
            return {};
        return CmdlineParsed { ai };
    }
    ~CmdlineParsed()
    {
        cmdline_parser_free(&ai);
    }
    auto& value() const { return ai; }

private:
    CmdlineParsed(gengetopt_args_info ai)
        : ai(ai)
    {
    }

    gengetopt_args_info ai;
};
}

tl::expected<ConfigParams, int> ConfigParams::from_args(int argc, char** argv)
{
    if (auto p { CmdlineParsed::parse(argc, argv) }) {
        ConfigParams c;
        if (auto i { c.init(p->value()) }; i < 1) {
            return tl::make_unexpected(i);
        }
        return c;
    }
    return tl::make_unexpected(-1);
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

TCPSockaddr fetch_endpointaddress(toml::node& n)
{
    auto p = TCPSockaddr::parse(fetch<std::string>(n));
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
    std::vector<Sockaddr> out;
    std::string::size_type pos = 0;
    while (true) {
        auto end = csv.find(",", pos);
        auto param = csv.substr(pos, end - pos);
        auto parsed = TCPSockaddr::parse(param);
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

int ConfigParams::init(const gengetopt_args_info& ai)
{
    const auto defaultDataDir { get_default_datadir() };
    bool dmp(ai.dump_config_given);
    if (!dmp)
        spdlog::info("Warthog Node v{}.{}.{} ", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

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
    std::optional<TCPSockaddr> nodeBind;
    std::optional<TCPSockaddr> rpcBind;
    std::optional<TCPSockaddr> publicrpcBind;
    std::optional<TCPSockaddr> stratumBind;
    node.isolated = ai.isolated_given;
    if (ai.testnet_given) {
        enable_testnet();
    }
    if (ai.enable_public_given) {
        publicrpcBind = TCPSockaddr("0.0.0.0:3001");
    }

    if (is_testnet()) {
        peers.connect = {
            "193.218.118.57:9286",
            "98.71.18.140:9286"
        };
    } else {
        peers.connect = {
            "193.218.118.57:9186",
            "96.41.20.26:9186",
            "89.163.224.253:9186",
            "88.11.56.103:9186",
            "81.163.20.40:9186",
            "74.122.131.1:9186",
            "68.227.255.200:9186",
            "64.92.35.4:9186",
            "37.114.63.165:9186",
            "15.235.162.190:9186",
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
                        } else if (k == "enable-ban") {
                            peers.enableBan = fetch<bool>(v);
                        } else if (k == "allow-localhost-ip") {
                            peers.allowLocalhostIp = fetch<bool>(v);
                        } else if (k == "log-communication") {
                            node.logCommunicationVal = fetch<bool>(v);
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
        auto p = TCPSockaddr::parse(ai.stratum_arg);
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
        auto p = TCPSockaddr::parse(ai.rpc_arg);
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
                jsonrpc.bind = TCPSockaddr::parse("127.0.0.1:3100").value();
            else
                jsonrpc.bind = TCPSockaddr::parse("127.0.0.1:3000").value();
        }
    }

    // JSON Public RPC socket
    if (ai.publicrpc_given) {
        auto p = TCPSockaddr::parse(ai.publicrpc_arg);
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
        auto p = TCPSockaddr::parse(ai.bind_arg);
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
                node.bind = TCPSockaddr::parse("0.0.0.0:9286").value();
            else
                node.bind = TCPSockaddr::parse("0.0.0.0:9186").value();
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

std::string ConfigParams::dump()
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
            { "enable-ban", peers.enableBan },
            { "allow-localhost-ip", peers.allowLocalhostIp },
            { "log-communication", (bool)node.logCommunicationVal } });
    tbl.insert_or_assign("db", toml::table {
                                   { "chain-db", data.chaindb },
                                   { "peers-db", data.peersdb },
                               });
    stringstream ss;
    ss << tbl << endl;
    return ss.str();
}

Config::Config(ConfigParams&& params)
    : ConfigParams(std::move(params))
{
    logCommunication = node.logCommunicationVal;
}
