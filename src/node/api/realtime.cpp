#include "realtime.hpp"
#include "block/header/difficulty.hpp"
#include "general/hex.hpp"
#include "mempool/log.hpp"
#include "nlohmann/json.hpp"
#include "transport/connection_base.hpp"
using nlohmann::json;

#ifdef DISABLE_LIBUV
#include "api/wasm/api.hpp"
namespace api = wasm_api;
#else
namespace api {
void on_connect_count(size_t)
{
}
void on_connect(json)
{
}
void on_disconnect(json)
{
}
void on_chain(json)
{
}
void on_mempool_add(json)
{
}
void on_mempool_erase(json)
{
}
}
#endif

namespace realtime_api {
void on_connect(size_t total, const ConnectionBase& c)
{

    return api::on_connect({ { "total", total },
        { "id", c.id },
        { "since", c.created_at_timestmap() },
        { "inbound", c.inbound() },
        { "type", c.type_str() },
        { "address", c.peer_addr().to_string() } });
}

void on_disconnect(size_t total, uint64_t id)
{
    return api::on_disconnect({ { "total", total }, { "id", id } });
}

void on_chain(OnChainEvent e)
{
    api::on_chain(
        {
            { "length", e.length.value() },
            { "target", e.target.hex_string() },
            { "difficulty", e.target.difficulty() },
            { "worksum", e.totalWork.getdouble() },
            { "worksumHex", e.totalWork.to_string() },
        });
}
void on_mempool_add(const mempool::Put& p, size_t total)
{
    auto& e { p.entry };
    api::on_mempool_add({ { "total", total },
        { "id", e.transaction_id().hex_string() },
        { "fromAddress", e.from_address().to_string() },
        { "pinHeight", e.pin_height().value() },
        { "txHash", e.tx_hash() },
        { "nonceId", e.nonce_id() },
        { "fee", e.fee().uncompact().to_string() },
        { "feeE8", e.fee().uncompact().E8() },
        { "toAddress", e.to_address().to_string() },
        { "amount", e.amount().to_string() },
        { "amountE8", e.amount().E8() } });
}

void on_mempool_erase(const mempool::Erase& e, size_t total)
{
    api::on_mempool_erase({ { "id", e.id.hex_string() }, { "total", total } });
}
}
