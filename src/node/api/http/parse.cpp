#include "parse.hpp"
#include "general/hex.hpp"
#include "general/reader.hpp"
#include "nlohmann/json.hpp"

using namespace nlohmann;
MiningTask parse_mining_task(const std::vector<uint8_t>& s)
{
    try {
        json parsed = json::parse(s);
        MiningTask mt {
            .block {
                .height { Height(parsed["height"].get<uint32_t>()).nonzero_throw(EBADHEIGHT) },
                .header { hex_to_arr<80>(parsed["header"].get<std::string>()) },
                .body { hex_to_vec(parsed["body"].get<std::string>()) },
            }
        };
        if (mt.block.body.size() > MAXBLOCKSIZE)
            throw Error(EBLOCKSIZE);
        return mt;
    } catch (const json::exception& e) {
        throw Error(EMALFORMED);
    }
}

namespace {
Height extract_pin_height(const nlohmann::json& json)
{
    try {
        auto h = json["pinHeight"].get<int>();
        if (h >= 0) {
            return Height(h);
        }
    } catch (...) {
    }
    throw Error(EBADHEIGHT);
}
NonceId extract_nonce_id(const nlohmann::json& json)
{
    try {
        return NonceId(json["nonceId"].get<uint32_t>());
    } catch (...) {
    }
    throw Error(EBADNONCE);
}

CompactUInt extract_fee(const nlohmann::json& json)
{
    try {
        std::optional<Funds> fee;
        auto iter = json.find("fee");
        if (iter != json.end()) {
            fee = { Funds::parse(iter->get<std::string>()) };
            if (!fee.has_value())
                goto error;
        }
        iter = json.find("feeE8");
        if (iter != json.end()) {
            if (fee.has_value()) 
                goto error; // exclusive, either "amount" or "amountE8"
            fee = Funds(iter->get<uint64_t>());
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
        return Address(json["toAddr"].get<std::string>());
    } catch (...) {
        throw Error(EBADADDRESS);
    }
}

Funds extract_funds(const nlohmann::json& json)
{
    try {
        std::optional<Funds> f;
        auto iter = json.find("amount");
        if (iter != json.end()) {
            f = Funds::parse(iter->get<std::string>());
            if (!f.has_value()) 
                goto error;
        }
        iter = json.find("amountE8");
        if (iter != json.end()) {
            if (f.has_value()) 
                goto error; // exclusive, either "amount" or "amountE8"
            f = Funds(iter->get<uint64_t>());
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
        signature = json["signature65"].get<std::string>();
    } catch (...) {
        throw Error(EPARSESIG);
    }
    return RecoverableSignature(signature);
}
}

PaymentCreateMessage parse_payment_create(const std::vector<uint8_t>& s)
{
    try {
        json parsed = json::parse(s);
        return PaymentCreateMessage(
            extract_pin_height(parsed), extract_nonce_id(parsed), NonceReserved::zero(), extract_fee(parsed), extract_to_addr(parsed), extract_funds(parsed), extract_signature(parsed));
    } catch (const json::exception& e) {
        throw Error(EMALFORMED);
    }
}

Funds parse_funds(const std::vector<uint8_t>& s)
{
    std::string str(s.begin(), s.end());
    if (auto o { Funds::parse(str) }; o.has_value())
        return *o;
    throw Error(EMALFORMED);
};
