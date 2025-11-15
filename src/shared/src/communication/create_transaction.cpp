#include "create_transaction.hpp"
#include "nlohmann/json.hpp"
#include "api/http/json_converter.hpp"
WartTransferCreate::operator std::string()
{
    return nlohmann::json {
        { "pinHeight", pin_height().value() },
        { "nonceId", nonce_id().value() },
        { "toAddr", to_addr().to_string() },
        { "amount", wart().to_string() },
        { "fee", compact_fee().to_string() },
        { "signature65", signature().to_string() },
    }
        .dump(1);
}


WartTransferCreate WartTransferCreate::parse_from(const JSONConverter& c)
{
    return { c, c, c, c, c, c };
}
TokenTransferCreate TokenTransferCreate::parse_from(const JSONConverter& c)
{
    return { c, c, c, c, c, c, c, c };
}

OrderCreate OrderCreate::parse_from(const JSONConverter& c)
{
    return { c, c, c, c, c, c, c, c };
};

LiquidityDepositCreate LiquidityDepositCreate::parse_from(const JSONConverter& c)
{
    return { c, c, c, c, c, c, c };
}

LiquidityWithdrawalCreate LiquidityWithdrawalCreate::parse_from(const JSONConverter& c)
{
    return { c, c, c, c, c, c };
}

CancelationCreate CancelationCreate::parse_from(const JSONConverter& c)
{
    return { c, c, c, c, c, c };
}
AssetCreationCreate AssetCreationCreate::parse_from(const JSONConverter& c)
{
    return { c, c, c, c, c, c };
}

TransactionCreate parse_transaction_create(const std::vector<uint8_t>& s)
{
    try {
        auto parsed { nlohmann::json::parse(s) };
        std::string type { parsed.at("type").get<std::string>() };
        return TransactionCreate::parse_from(type, parsed);
    } catch (const nlohmann::json::exception& e) {
        throw Error(ETXTYPE);
    }
}
