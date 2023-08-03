#include "create_payment.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/reader.hpp"

PaymentCreateMessage::PaymentCreateMessage(
    PinHeight pinHeight, const Hash& pinHash,
    const PrivKey& privateKey, CompactUInt fee,
    const Address& toAddress, Funds amount, NonceId nonceId)
    : pinHeight(pinHeight)
    , nonceId(nonceId)
    , reserved { NonceReserved::zero()}
    , compactFee(fee)
    , toAddr(toAddress)
    , amount(amount)
    , signature(privateKey.sign(tx_hash(pinHash))) {};

bool PaymentCreateMessage::valid_signature(HashView pinHash, AddressView fromAddress){
    return signature.recover_pubkey(tx_hash(pinHash)).address() == fromAddress;
};

PaymentCreateMessage::PaymentCreateMessage(ReaderCheck<bytesize> r)
    : pinHeight(Height(r.r))
    , nonceId(r.r)
    , reserved(r.r)
    , compactFee(r.r)
    , toAddr(r.r)
    , amount(r)
    , signature(r.r) {};

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
};
TxHash PaymentCreateMessage::tx_hash(HashView pinHash) const
{
    return TxHash(HasherSHA256()
        << pinHash
        << pinHeight
        << nonceId
        << reserved
        << compactFee
        << toAddr
        << amount);
};

PaymentCreateMessage::operator std::vector<uint8_t>()
{
    std::vector<uint8_t> out(bytesize);
    Writer w(out);
    w << *this;
    assert(w.remaining() == 0);
    return out;
};
Address PaymentCreateMessage::from_address(
    HashView txHash) const
{
    return signature.recover_pubkey(txHash.data()).address();
};
