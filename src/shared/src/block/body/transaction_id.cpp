#include "transaction_id.hpp"
#include "block/chain/height.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"

Writer& operator<<(Writer& w, const TransactionId& id)
{
    return w
        << id.accountId
        << id.pinHeight
        << id.nonceId;
};
TransactionId::TransactionId(Reader& r)
    : accountId(r.uint64())
    , pinHeight(Height(r.uint32()))
    , nonceId(r.uint32()) {};

TxidWithFee::TxidWithFee(Reader& r)
    : txid(r)
    , fee(r)
{
}

Writer& operator<<(Writer& w, const TxidWithFee& twf)
{
    return w << twf.txid << twf.fee;
}
