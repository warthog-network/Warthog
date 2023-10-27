#include "api/interface.hpp"
#include "general/errors.hpp"
#include "nlohmann/json.hpp"
struct Head;
class Hash;
class Header;
namespace jsonmsg {

nlohmann::json to_json(const API::Balance&);
nlohmann::json to_json(const Grid&);
nlohmann::json to_json(const Hash&);
nlohmann::json to_json(const API::Head&);
nlohmann::json to_json(const Header&);
nlohmann::json to_json(const MiningTask&);
nlohmann::json to_json(const API::MempoolEntries&);
nlohmann::json to_json(const API::Transaction&);
nlohmann::json to_json(const API::TransactionsByBlocks&);
nlohmann::json to_json(const API::Block&);
nlohmann::json to_json(const API::AccountHistory&);
nlohmann::json to_json(const API::Richlist&);
nlohmann::json to_json(const API::HashrateInfo&);
nlohmann::json to_json(const OffenseEntry& e);
nlohmann::json to_json(const std::optional<SignedSnapshot>&);
nlohmann::json to_json(const chainserver::TransactionIds&);
nlohmann::json to_json(const API::Round16Bit&);
template <typename T>
inline nlohmann::json to_json(const std::vector<T>& e)
{
    nlohmann::json j = nlohmann::json::array();
    for (auto& item : e) {
        j.push_back(to_json(item));
    }
    return nlohmann::json { { "data", j } };
}

inline std::string status(int32_t e)
{
    nlohmann::json j;
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
    return nlohmann::json {
        { "code", 0 },
        { "data", to_json(*e) }
    }.dump(1);
}

inline std::string serialize(const std::vector<PeerDB::BanEntry>& banned)
{
    nlohmann::json j = nlohmann::json::array();
    for (auto& item : banned) {
        nlohmann::json elem;
        elem["ip"] = item.ip.to_string().c_str();
        elem["expires"] = item.banuntil;
        elem["reason"] = item.offense.err_name();
        j.push_back(elem);
    }
    return j.dump(1);
}

std::string serialize(const std::vector<API::Peerinfo>& banned);

std::string endpoints(const Eventloop&);
std::string connect_timers(const Eventloop&);
std::string header_download(const Eventloop&);
std::string ip_counter(const Conman&);


}
