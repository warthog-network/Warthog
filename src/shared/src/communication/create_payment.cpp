#include "create_payment.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
#include "nlohmann/json.hpp"

PaymentCreateMessage::PaymentCreateMessage(
    PinHeight pinHeight, const Hash& pinHash,
    const PrivKey& privateKey, CompactUInt fee,
    const Address& toAddress, Funds amount, NonceId nonceId)
    : pinHeight(pinHeight)
    , nonceId(nonceId)
    , reserved { NonceReserved::zero() }
    , compactFee(fee)
    , toAddr(toAddress)
    , amount(amount)
    , signature(privateKey.sign(tx_hash(pinHash)))
{
}

PaymentCreateMessage::PaymentCreateMessage(ReaderCheck<bytesize> r)
    : pinHeight(Height(r.r))
    , nonceId(r.r)
    , reserved(r.r)
    , compactFee(CompactUInt::from_value_throw(r.r))
    , toAddr(r.r)
    , amount(Funds::from_value_throw(r.r))
    , signature(r.r)
{
    r.assert_read_bytes();
}

Writer& operator<<(Writer& w, const PaymentCreateMessage& m)
{
    return w
        << m.pinHeight
        << m.nonceId
        << m.reserved
        << m.compactFee
        << m.toAddr
        << m.amount
        << m.signature;
}

TxHash PaymentCreateMessage::tx_hash(HashView pinHash) const
{
    return TxHash(HasherSHA256()
        << pinHash
        << pinHeight
        << nonceId
        << reserved
        << compactFee.uncompact() 
        << toAddr
        << amount);
}

PaymentCreateMessage::operator std::vector<uint8_t>()
{
    std::vector<uint8_t> out(bytesize);
    Writer w(out);
    w << *this;
    assert(w.remaining() == 0);
    return out;
}

PaymentCreateMessage::operator std::string()
{
    return nlohmann::json {
        { "pinHeight", pinHeight.value() },
        { "nonceId", nonceId.value() },
        { "toAddr", toAddr.to_string() },
        { "amount", amount.to_string() },
        { "fee", compactFee.to_string() },
        { "signature65", signature.to_string() },
    }
        .dump(1);
}

bool PaymentCreateMessage::valid_signature(HashView pinHash, AddressView fromAddress) const
{
    return signature.recover_pubkey(tx_hash(pinHash)).address() == fromAddress;
}

Address PaymentCreateMessage::from_address(
    HashView txHash) const
{
    return signature.recover_pubkey(txHash.data()).address();
}

TokenPaymentCreateMessage::TokenPaymentCreateMessage(
    PinHeight pinHeight, const Hash& pinHash,
    const PrivKey& privateKey, Hash tokenHash, CompactUInt fee,
    const Address& toAddress, Funds amount, NonceId nonceId)
    : pinHeight(pinHeight)
    , nonceId(nonceId)
    , reserved { NonceReserved::zero() }
    , tokenHash(std::move(tokenHash))
    , compactFee(fee)
    , toAddr(toAddress)
    , amount(amount)
    , signature(privateKey.sign(tx_hash(pinHash)))
{
}

TokenPaymentCreateMessage::TokenPaymentCreateMessage(ReaderCheck<bytesize> r)
    : pinHeight(Height(r.r))
    , nonceId(r.r)
    , reserved(r.r)
    , tokenHash(r.r)
    , compactFee(CompactUInt::from_value_throw(r.r))
    , toAddr(r.r)
    , amount(Funds::from_value_throw(r.r))
    , signature(r.r)
{
    r.assert_read_bytes();
}

Writer& operator<<(Writer& w, const TokenPaymentCreateMessage& m)
{
    return w
        << m.pinHeight
        << m.nonceId
        << m.reserved
        << m.tokenHash
        << m.compactFee
        << m.toAddr
        << m.amount
        << m.signature;
}

TxHash TokenPaymentCreateMessage::tx_hash(HashView pinHash) const
{
    return TxHash(HasherSHA256()
        << pinHash
        << pinHeight
        << nonceId
        << reserved
        << tokenHash
        << compactFee.uncompact()
        << toAddr
        << amount);
}

TokenPaymentCreateMessage::operator std::vector<uint8_t>()
{
    std::vector<uint8_t> out(bytesize);
    Writer w(out);
    w << *this;
    assert(w.remaining() == 0);
    return out;
}

TokenPaymentCreateMessage::operator std::string()
{
    return nlohmann::json {
        { "pinHeight", pinHeight.value() },
        { "nonceId", nonceId.value() },
        { "tokenHash", tokenHash.hex_string() },
        { "toAddr", toAddr.to_string() },
        { "amount", amount.to_string() },
        { "fee", compactFee.to_string() },
        { "signature65", signature.to_string() },
    }
        .dump(1);
}

bool TokenPaymentCreateMessage::valid_signature(HashView pinHash, AddressView fromAddress) const
{
    return from_address(tx_hash(pinHash)) == fromAddress;
}

Address TokenPaymentCreateMessage::from_address(
    HashView txHash) const
{
    return signature.recover_pubkey(txHash).address();
}
