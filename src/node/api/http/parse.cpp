#include "parse.hpp"
#include "block/body/container.hpp"
#include "block/header/header_impl.hpp"
#include "general/hex.hpp"
#include "nlohmann/json.hpp"
namespace {
using nlohmann::json;
template <typename T>
wrt::optional<T> get_optional(const json& j, std::string_view key)
{
    auto it { j.find(key) };
    if (it == j.end())
        return {};
    return it->get<T>();
}
}

using namespace nlohmann;
BlockWorker parse_block_worker(const std::vector<uint8_t>& s)
{
    try {
        json parsed = json::parse(s);

        auto height { Height(parsed.at("height").get<uint32_t>()).nonzero_throw(EBADHEIGHT) };
        Header header { hex_to_arr<80>(parsed.at("header").get<std::string>()) };
        VersionedBodyData bd { BodyData(hex_to_vec(parsed.at("body").get<std::string>())), header.version() };
        BlockWorker mt {
            .block { height, header, Body::parse_throw(std::move(bd), height) },
            .worker { get_optional<std::string>(parsed, "worker").value_or(std::string()) }
        };
        return mt;
    } catch (const json::exception& e) {
        throw Error(EINV_ARGS);
    }
}

namespace {
PinHeight extract_pin_height(const nlohmann::json& json)
{
    try {
        auto h = json.at("pinHeight").get<int>();
        if (h >= 0) {
            return PinHeight(Height(h));
        }
    } catch (...) {
    }
    throw Error(EBADHEIGHT);
}
NonceId extract_nonce_id(const nlohmann::json& json)
{
    try {
        return NonceId(json.at("nonceId").get<uint32_t>());
    } catch (...) {
    }
    throw Error(EBADNONCE);
}

CompactUInt extract_fee(const nlohmann::json& json)
{
    try {
        wrt::optional<Wart> fee;
        auto iter = json.find("fee");
        if (iter != json.end()) {
            fee = { Wart::parse_throw(iter->get<std::string>()) };
            if (!fee.has_value())
                goto error;
        }
        iter = json.find("feeE8");
        if (iter != json.end()) {
            if (fee.has_value())
                goto error; // exclusive, either "amount" or "amountE8"
            fee = Wart::from_value(iter->get<uint64_t>());
            if (!fee.has_value())
                goto error;
        }

        auto compactFee { CompactUInt::compact(*fee) };
        bool exact = compactFee.uncompact() == *fee;
        if (!exact)
            throw Error(EINEXACTFEE);
        return compactFee;
    } catch (json::exception&) {
    }
error:
    throw Error(EBADFEE);
}

Address extract_to_addr(const nlohmann::json& json)
{
    try {
        return Address(json.at("toAddr").get<std::string>());
    } catch (...) {
        throw Error(EBADADDRESS);
    }
}

Wart extract_wart(const nlohmann::json& json)
{
    try {
        wrt::optional<Wart> f;
        auto iter = json.find("amount");
        if (iter != json.end()) {
            f = Wart::parse(iter->get<std::string>());
            if (!f.has_value())
                goto error;
        }
        iter = json.find("amountE8");
        if (iter != json.end()) {
            if (f.has_value())
                goto error; // exclusive, either "amount" or "amountE8"
            f = Wart::from_value(iter->get<uint64_t>());
            if (!f.has_value())
                goto error;
        }

        if (f.has_value())
            return f.value();
    } catch (...) {
    }
error:
    throw Error(EBADAMOUNT);
}

RecoverableSignature extract_signature(const nlohmann::json& json)
{
    std::string signature;
    try {
        signature = json.at("signature65").get<std::string>();
    } catch (...) {
        throw Error(EPARSESIG);
    }
    return RecoverableSignature(signature);
}
}

WartTransferCreate parse_payment_create(const std::vector<uint8_t>& s)
{
    try {
        json parsed = json::parse(s);
        return WartTransferCreate(
            extract_pin_height(parsed), extract_nonce_id(parsed), NonceReserved::zero(), extract_fee(parsed), extract_to_addr(parsed), extract_wart(parsed).nonzero_throw(), extract_signature(parsed));
    } catch (const json::exception& e) {
        throw Error(EINV_ARGS);
    }
}

// Funds_uint64 parse_funds(const std::vector<uint8_t>& s)
// {
//     std::string str(s.begin(), s.end());
//     if (auto o { Funds_uint64::parse(str) }; o.has_value())
//         return *o;
//     throw Error(EINV_ARGS);
// };
