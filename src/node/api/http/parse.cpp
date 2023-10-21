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
CompactUInt extract_fee(const nlohmann::json& json, bool strict)
{
    try {
        auto fee { Funds::parse(json["feeE8"].get<std::string>()) };
        if (!fee.has_value()) 
            goto error;
        auto compactFee { CompactUInt::compact(*fee) };
        if (strict) {
            bool exact = compactFee.uncompact() == *fee;
            if (!exact)
                throw Error(ESTRICTFEE);
        }
        return compactFee;
    } catch (...) {
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
        auto f{Funds::parse(json["amountE8"].get<std::string>())};
        if (f.has_value()) 
            return f.value();
    } catch (...) {
    }
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
        bool strict = false;
        ;
        if (auto iter = parsed.find("strict"); iter != parsed.end()) {
            auto& val { (*iter) };
            if (!val.is_boolean())
                throw Error(EMALFORMED);
            strict = val.get<bool>();
        }
        return PaymentCreateMessage(
            extract_pin_height(parsed), extract_nonce_id(parsed), NonceReserved::zero(), extract_fee(parsed, strict), extract_to_addr(parsed), extract_funds(parsed), extract_signature(parsed));
    } catch (const json::exception& e) {
        throw Error(EMALFORMED);
    }
}
