#include "create_transaction.hpp"
#include "nlohmann/json.hpp"
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
