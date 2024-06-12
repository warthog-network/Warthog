#include "transaction_id.hpp"
#include "block/chain/height.hpp"
#include "general/hex.hpp"
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

std::string TransactionId::hex_string() const
{
    std::array<uint8_t, bytesize> bytes;
    Writer w(bytes.data(), bytesize);
    w << *this;
    return serialize_hex(bytes);
}

TxidWithFee::TxidWithFee(Reader& r)
    : txid(r)
    , fee(r)
{
}

Writer& operator<<(Writer& w, const TxidWithFee& twf)
{
    return w << twf.txid << twf.fee;
}
