#include "create_payment.hpp"
#include "nlohmann/json.hpp"
WartTransferCreate::operator std::string()
{
    return nlohmann::json {
        { "pinHeight", pinHeight.value() },
        { "nonceId", nonceId.value() },
        { "toAddr", to_addr().to_string() },
        { "amount", wart().to_string() },
        { "fee", compactFee.to_string() },
        { "signature65", signature().to_string() },
    }
        .dump(1);
}
