#include "config.hpp"
#include "browser.hpp"
#include "cmdline/cmdline.hpp"
#include "general/errors.hpp"
#include "general/is_testnet.hpp"
#include "spdlog/spdlog.h"
#include "toml++/toml.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
#include "version.hpp"
#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#ifdef __APPLE__
#include <sys/types.h>
#include <unistd.h>
#endif
#ifdef __linux__
#include <pwd.h>
#endif

using namespace std;

#ifndef DISABLE_LIBUV
std::string ConfigParams::get_default_datadir()
{
#ifdef __linux__
    const char* osBaseDir = nullptr;
    if ((osBaseDir = getenv("HOME")) == NULL) {
        osBaseDir = getpwuid(getuid())->pw_dir;
    }
    if (osBaseDir == nullptr)
        throw std::runtime_error("Cannot determine default data directory.");
    return std::string(osBaseDir) + "/.warthog/";
#elif _WIN32
    const char* osBaseDir = nullptr;
    osBaseDir = getenv("LOCALAPPDATA");
    if (osBaseDir == nullptr)
        throw std::runtime_error("Cannot determine default data directory.");
    return std::string(osBaseDir) + "/Warthog/";
#elif __APPLE__
    const char* osBaseDir = nullptr;
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

#else

#include <emscripten.h>
#include <emscripten/wasmfs.h>
#include <unistd.h>
bool ConfigParams::mount_opfs(const char* mountpoint)
{
    spdlog::info("Mounting OPFS at {}", mountpoint);
    auto pOpfs = wasmfs_create_opfs_backend();
    if (!pOpfs)
        return false;
    bool exists = access(mountpoint, F_OK) == 0;
    if (exists)
        return true;

    const int rc = wasmfs_create_directory(mountpoint, 0777, pOpfs);
    return rc == 0;
}
std::string ConfigParams::get_default_datadir()
{
    return "/opfs/";
}
#endif

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
    CmdlineParsed(const CmdlineParsed&) = delete;
    CmdlineParsed(CmdlineParsed&& other)
        : ai(other.ai)
    {
        other.deleteOnDestruction = false;
    };
    ~CmdlineParsed()
    {
        if (deleteOnDestruction) {
            cmdline_parser_free(&ai);
        }
    }
    auto& value() const { return ai; }

private:
    CmdlineParsed(gengetopt_args_info& ai0)
        : ai(ai0)
    {
    }

    bool deleteOnDestruction { true };
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

TCPPeeraddr fetch_endpointaddress(toml::node& n)
{
    auto p = TCPPeeraddr::parse(fetch<std::string>(n));
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
    std::vector<TCPPeeraddr> out;
#ifndef DISABLE_LIBUV
    std::string::size_type pos = 0;
    while (true) {
        auto end = csv.find(",", pos);
        auto param = csv.substr(pos, end - pos);
        auto parsed = TCPPeeraddr::parse(param);
        if (!parsed) {
            throw std::runtime_error("Invalid parameter '"s + param + "'."s);
        }
        out.push_back(parsed.value());
        if (end == std::string::npos) {
            break;
        }
        pos = end + 1;
    }
#endif
    return out;
}
} // namespace

int ConfigParams::init(const gengetopt_args_info& ai)
{
    bool dmp(ai.dump_config_given);
    if (!dmp)
        spdlog::info("Warthog Node v{}.{}.{} ", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

        // Log
#ifdef DISABLE_LIBUV
    assert(ConfigParams::mount_opfs("/opfs"));
#endif
    const auto defaultDataDir { get_default_datadir() };
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
    std::optional<TCPPeeraddr> nodeBind;
    std::optional<TCPPeeraddr> rpcBind;
    std::optional<TCPPeeraddr> publicrpcBind;
    std::optional<TCPPeeraddr> stratumBind;
    node.isolated = ai.isolated_given;
    node.disableTxsMining = ai.disable_tx_mining_given;
    if (ai.testnet_given) {
        enable_testnet();
    }
    if (ai.enable_public_given) {
        publicrpcBind = TCPPeeraddr("0.0.0.0:3001");
    }

#ifndef DISABLE_LIBUV
    if (is_testnet()) {
        peers.connect = EndpointVector {
            "193.218.118.57:9286",
            "98.71.18.140:9286"
        };
    } else {
        peers.connect = EndpointVector {
            "122.148.197.165:9186",
            "135.181.77.214:9186",
            "167.114.1.208:9186",
            "185.209.228.16:9186",
            "185.215.180.7:9186",
            "193.218.118.57:9186",
            "194.164.30.182:9186",
            "203.25.209.147:9186",
            "209.12.214.158:9186",
            "213.199.59.252:20016",
            "47.187.202.183:9186",
            "49.13.161.201:9186",
            "51.75.21.134:9186",
            "62.72.44.89:9186",
            "63.231.144.31:9186",
            "74.208.75.230:9186",
            "74.208.77.165:9186",
            "81.163.20.40:9186",
            "82.146.46.246:9186",
            "89.107.33.239:9186",
            "89.117.150.162:9186",
            "89.163.224.253:9186",
        };
    }
#endif

    std::string filename = is_testnet() ? "testnet_config.toml" : "config.toml";
    if (!ai.config_given && !std::filesystem::exists(filename)) {
        if (!dmp)
            spdlog::debug("No config.toml file found, using default configuration");
        if (ai.test_given) {
            spdlog::error("No configuration file found.");
            return -1;
        }
    } else {
#ifndef DISABLE_LIBUV
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
#endif
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
            data.peersdb = defaultDataDir + (is_testnet() ? "testnet_peers.db3" : "peers_v2.db3");
        }
    }
    if (ai.temporary_given)
        data.chaindb = "";

    // Stratum API socket
    if (ai.stratum_given) {
        auto p = TCPPeeraddr::parse(ai.stratum_arg);
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
        auto p = TCPPeeraddr::parse(ai.rpc_arg);
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
                jsonrpc.bind = TCPPeeraddr::parse("127.0.0.1:3100").value();
            else
                jsonrpc.bind = TCPPeeraddr::parse("127.0.0.1:3000").value();
        }
    }

    // JSON Public RPC socket
    if (ai.publicrpc_given) {
        auto p = TCPPeeraddr::parse(ai.publicrpc_arg);
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
        auto p = TCPPeeraddr::parse(ai.bind_arg);
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
                node.bind = TCPPeeraddr::parse("0.0.0.0:9286").value();
            else
                node.bind = TCPPeeraddr::parse("0.0.0.0:9186").value();
        }
    }
#ifndef DISABLE_LIBUV
    if (ai.connect_given) {
        peers.connect = parse_endpoints(ai.connect_arg);
    }
#else
    auto wsPeers { ws_peers() };
    spdlog::info("Websocket default peer list ({}):", wsPeers.size());

    size_t i = 1;
    for (auto& p : wsPeers) {
        auto a { WSUrladdr::parse(p) };
        if (a) {
            spdlog::info("Adding websocket peer {}: {}", i, p);
            peers.connect.push_back(*a);
        } else {
            spdlog::warn("Failed parsing Websocket peer {}: {}", i, p);
        }
        i += 1;
    }
#endif

    if (ai.ws_port_given) {
        auto parse_port = [](int port) -> uint16_t {
            if (port < 0 || port > std::numeric_limits<uint16_t>::max()) {
                throw std::runtime_error("Invalid port '" + std::to_string(port) + "'");
            }
            return port;
        };
        websocketServer.port = parse_port(ai.ws_port_arg);
        if (ai.ws_tls_key_given)
            websocketServer.keyfile = ai.ws_tls_key_arg;
        if (ai.ws_tls_cert_given)
            websocketServer.certfile = ai.ws_tls_cert_arg;
        if (ai.ws_x_forwarded_for_given)
            websocketServer.XFowarded = true;
        if (ai.ws_bind_localhost_given)
            websocketServer.bindLocalhost = true;
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
#ifndef DISABLE_LIBUV
    for (auto ea : peers.connect) {
        connect.push_back(ea.to_string());
    }
#endif
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
