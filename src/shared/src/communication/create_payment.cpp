#include "create_payment.hpp"
#include "crypto/hasher_sha256.hpp"
#include "nlohmann/json.hpp"
#include "general/writer.hpp"

WartTransferCreate::WartTransferCreate(
    PinHeight pinHeight, const Hash& pinHash,
    const PrivKey& privateKey, CompactUInt fee,
    const Address& toAddress, Wart amount, NonceId nonceId)
    : pinHeight(pinHeight)
    , nonceId(nonceId)
    , reserved { NonceReserved::zero() }
    , compactFee(fee)
    , toAddr(toAddress)
    , amount(amount)
    , signature(privateKey.sign(tx_hash(pinHash)))
{
}

TxHash WartTransferCreate::tx_hash(HashView pinHash) const
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

WartTransferCreate::operator std::string()
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

bool WartTransferCreate::valid_signature(HashView pinHash, AddressView fromAddress) const
{
    return signature.recover_pubkey(tx_hash(pinHash)).address() == fromAddress;
}

Address WartTransferCreate::from_address(
    HashView txHash) const
{
    return signature.recover_pubkey(txHash.data()).address();
}

TokenTransferCreate::TokenTransferCreate(
    PinHeight pinHeight, const Hash& pinHash,
    const PrivKey& privateKey, Hash tokenHash, CompactUInt fee,
    const Address& toAddress, ParsedFunds amount, NonceId nonceId)
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


TxHash TokenTransferCreate::tx_hash(HashView pinHash) const
{
    return TxHash(HasherSHA256()
        << pinHash
        << pinHeight
        << nonceId
        << reserved
        << tokenHash
        << compactFee.uncompact()
        << toAddr
        << amount.v);
}

TokenTransferCreate::operator std::string()
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

bool TokenTransferCreate::valid_signature(HashView pinHash, AddressView fromAddress) const
{
    return from_address(tx_hash(pinHash)) == fromAddress;
}

Address TokenTransferCreate::from_address(
    HashView txHash) const
{
    return signature.recover_pubkey(txHash).address();
}
