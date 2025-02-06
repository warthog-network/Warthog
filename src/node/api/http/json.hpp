#include "api/interface.hpp"
#include "general/errors.hpp"
#include "nlohmann/json.hpp"
struct Head;
class Hash;
class TxHash;
class Header;
namespace jsonmsg {
using namespace nlohmann;
json to_json(const PeerDB::BanEntry&);
json to_json(const API::Peerinfo&);
json to_json(const API::Balance&);
json to_json(const Grid&);
json to_json(const NodeVersion&);
json to_json(const Hash&);
json to_json(const TxHash&);
json to_json(const API::Head&);
json to_json(const EndpointAddress&);
json to_json(const std::pair<NonzeroHeight, Header>&);
json to_json(const API::MiningState&);
json to_json(const API::MempoolEntries&);
json to_json(const API::Transaction&);
json to_json(const API::PeerinfoConnections&);
json to_json(const API::TransactionsByBlocks&);
json to_json(const API::Block&);
json to_json(const API::AccountHistory&);
json to_json(const API::Richlist&);
json to_json(const API::Wallet&);
json to_json(const API::HashrateInfo&);
json to_json(const API::HashrateChart&);
json to_json(const OffenseEntry& e);
json to_json(const std::optional<SignedSnapshot>&);
json to_json(const chainserver::TransactionIds&);
json to_json(const API::Round16Bit&);
json to_json(const API::Rollback&);

template <typename T>
inline json to_json(const std::vector<T>& e, const auto& map)
{
    json j = json::array();
    for (auto& item : e) {
        j.push_back(to_json(map(item)));
    }
    return j;
}

template <typename T>
inline json to_json(const std::vector<T>& e)
{
    return to_json(e, std::identity());
}

inline std::string status(int32_t e)
{
    json j;
    j["code"] = e;
    if (e == 0) {
        j["error"] = nullptr;
    } else {
        j["error"] = Error(e).strerror();
    }
    return j.dump(1);
}

inline std::string status(const tl::expected<void, int32_t>& e)
{
    if (e.has_value()) {
        return status(0);
    } else {
        return status(e.error());
    }
}

inline std::string serialize(const tl::expected<void, int32_t>& e)
{
    if (e.has_value()) {
        return status(0);
    } else {
        return status(e.error());
    }
}

template <typename T>
std::string serialize(const tl::expected<T, int32_t>& e)
{
    if (!e.has_value())
        return status(e.error());
    return json {
        { "code", 0 },
        { "data", to_json(*e) }
    }.dump(1);
}

template <typename T>
std::string serialize(const tl::expected<T, Error>& e)
{
    if (!e.has_value())
        return status(e.error().e);
    return json {
        { "code", 0 },
        { "data", to_json(*e) }
    }.dump(1);
}

inline std::string serialize(const tl::unexpected<int> e)
{
    return status(e.value());
}
std::string serialize(const API::Raw& r);

template <typename T>
inline std::string serialize(T&& e)
{
    return json {
        { "code", 0 },
        { "data", to_json(std::forward<T>(e)) }
    }.dump(1);
}

std::string endpoints(const Eventloop&);
std::string connect_timers(const Eventloop&);
std::string header_download(const Eventloop&);
std::string ip_counter(const Conman&);

}
