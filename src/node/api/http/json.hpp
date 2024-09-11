#pragma once
#include "api/interface.hpp"
#include "general/errors.hpp"
#include "nlohmann/json.hpp"
struct Head;
class Hash;
class TxHash;
class Header;
namespace jsonmsg {

nlohmann::json to_json(const api::Balance&);
nlohmann::json to_json(const Grid&);
nlohmann::json to_json(const NodeVersion&);
nlohmann::json to_json(const Hash&);
nlohmann::json to_json(const TxHash&);
nlohmann::json to_json(const api::Head&);
nlohmann::json to_json(const Peeraddr&);
nlohmann::json to_json(const std::pair<NonzeroHeight,Header>&);
nlohmann::json to_json(const api::MiningState&);
nlohmann::json to_json(const api::MempoolEntries&);
nlohmann::json to_json(const api::Transaction&);
nlohmann::json to_json(const api::PeerinfoConnections&);
nlohmann::json to_json(const api::TransactionsByBlocks&);
nlohmann::json to_json(const api::Block&);
nlohmann::json to_json(const api::AccountHistory&);
nlohmann::json to_json(const api::Richlist&);
nlohmann::json to_json(const api::Wallet&);
nlohmann::json to_json(const api::HashrateInfo&);
nlohmann::json to_json(const api::HashrateChart&);
nlohmann::json to_json(const OffenseEntry& e);
nlohmann::json to_json(const std::optional<SignedSnapshot>&);
nlohmann::json to_json(const chainserver::TransactionIds&);
nlohmann::json to_json(const api::Round16Bit&);
nlohmann::json to_json(const api::Rollback&);

template <typename T>
inline nlohmann::json to_json(const std::vector<T>& e, const auto& map)
{
    nlohmann::json j = nlohmann::json::array();
    for (auto& item : e) {
        j.push_back(to_json(map(item)));
    }
    return nlohmann::json { { "data", j } };
}

template <typename T>
inline nlohmann::json to_json(const std::vector<T>& e)
{
    return to_json(e,std::identity());
}

inline std::string status(Error e)
{
    nlohmann::json j;
    j["code"] = e.code;
    if (e.is_error()) {
        j["error"] = nullptr;
    } else {
        j["error"] = e.strerror();
    }
    return j.dump(1);
}


inline std::string status(const tl::expected<void, Error>& e)
{
    if (e.has_value()) {
        return status(0);
    } else {
        return status(e.error());
    }
}

inline std::string serialize(const tl::expected<void, Error>& e)
{
    if (e.has_value()) {
        return status(0);
    } else {
        return status(e.error());
    }
}


template <typename T>
std::string serialize(const tl::expected<T, Error>& e)
{
    if (!e.has_value())
        return status(e.error());
    return nlohmann::json {
        { "code", 0 },
        { "data", to_json(*e) }
    }.dump(1);
}

inline std::string serialize(const tl::unexpected<Error> e)
{
    return status(e.value());
}
std::string serialize(const api::Raw& r);

template<typename T>
inline std::string serialize(T&& e){
    return nlohmann::json {
        { "code", 0 },
        { "data", to_json(std::forward<T>(e)) }
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

std::string serialize(const std::vector<api::Peerinfo>& banned);

std::string endpoints(const Eventloop&);
// std::string connect_timers(const Eventloop&);
std::string header_download(const Eventloop&);


}
