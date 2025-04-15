#include "communication/create_payment.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/writer.hpp"
#include "mempool/entry.hpp"
#include "parse.hpp"

Writer& operator<<(Writer& w, WartTransferMessage m)
{
    return w << m.txid
             << m.reserved
             << m.compactFee
             << m.toAddr
             << m.amount
             << m.signature;
}

Address WartTransferMessage::from_address(HashView txHash) const
{
    return signature.recover_pubkey(txHash.data()).address();
}

TxHash WartTransferMessage::txhash(HashView pinHash) const
{
    return TxHash(
        HasherSHA256()
        << pinHash
        << pin_height()
        << txid.nonceId
        << reserved
        << compactFee.uncompact()
        << toAddr
        << amount);
}

WartTransferMessage::WartTransferMessage(WartTransferView t, PinHeight ph, AddressView toAddr)
    : txid(t.txid(ph))
    , reserved(t.pin_nonce().reserved)
    , compactFee(t.compact_fee_throw())
    , toAddr(toAddr)
    , amount(t.amount_throw())
    , signature(t.signature())
{
}

WartTransferMessage::WartTransferMessage(AccountId fromId, const WartPaymentCreateMessage& pcm)
    : txid(fromId, pcm.pinHeight, pcm.nonceId)
    , reserved(pcm.reserved)
    , compactFee(pcm.compactFee)
    , toAddr(pcm.toAddr)
    , amount(pcm.amount)
    , signature(pcm.signature)
{
}

WartTransferMessage::WartTransferMessage(const TransactionId& txid, const mempool::entry::Value& v)
    : txid(txid)
    , reserved(v.noncep2)
    , compactFee(v.fee)
    , toAddr(v.toAddr)
    , amount(v.amount)
    , signature(v.signature)
{
}

WartTransferMessage::WartTransferMessage(ReaderCheck<bytesize> r)
    : txid(r.r)
    , reserved(r.r.view<3>())
    , compactFee(CompactUInt::from_value_throw(r.r.uint16()))
    , toAddr(r.r.view<AddressView>())
    , amount(Funds_uint64::from_value_throw(r.r.uint64()))
    , signature(r.r.view<65>())
{
    r.assert_read_bytes();
}

Writer& operator<<(Writer& w, TransferDefiMessage m)
{
    return w << m.txid
             << m.reserved
             << m.compactFee
             << m.tokenHash
             << m.toAddr
             << m.amount
             << m.signature;
}

Address TransferDefiMessage::from_address(HashView txHash) const
{
    return signature.recover_pubkey(txHash.data()).address();
}

TxHash TransferDefiMessage::txhash(HashView pinHash) const
{
    return TxHash(
        HasherSHA256()
        << pinHash
        << pin_height()
        << txid.nonceId
        << reserved
        << compactFee.uncompact()
        << tokenHash
        << toAddr
        << amount);
}

TransferDefiMessage::TransferDefiMessage(WartTransferView t, Hash tokenHash, PinHeight ph, AddressView toAddr)
    : txid(t.txid(ph))
    , reserved(t.pin_nonce().reserved)
    , compactFee(t.compact_fee_throw())
    , tokenHash(std::move(tokenHash))
    , toAddr(toAddr)
    , amount(t.amount_throw())
    , signature(t.signature())
{
}

TransferDefiMessage::TransferDefiMessage(AccountId fromId, const TokenPaymentCreateMessage& pcm)
    : txid(fromId, pcm.pinHeight, pcm.nonceId)
    , reserved(pcm.reserved)
    , compactFee(pcm.compactFee)
    , tokenHash(pcm.tokenHash)
    , toAddr(pcm.toAddr)
    , amount(pcm.amount)
    , signature(pcm.signature)
{
}

TransferDefiMessage::TransferDefiMessage(const TransactionId& txid, const mempool::EntryValue& v)
    : txid(txid)
    , reserved(v.noncep2)
    , compactFee(v.fee)
    , tokenHash(v.tokenHash)
    , toAddr(v.toAddr)
    , amount(v.amount)
    , signature(v.signature)
{
}

TransferDefiMessage::TransferDefiMessage(ReaderCheck<bytesize> r)
    : txid(r.r)
    , reserved(r.r.view<3>())
    , compactFee(CompactUInt::from_value_throw(r.r.uint16()))
    , tokenHash(r.r)
    , toAddr(r.r.view<AddressView>())
    , amount(Funds_uint64::from_value_throw(r.r.uint64()))
    , signature(r.r.view<65>())
{
    r.assert_read_bytes();
}
