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

WartTransferMessage::WartTransferMessage(const TransactionId& txid, const mempool::entry::Shared& s, const mempool::entry::WartTransfer& t)
    : txid(txid)
    , reserved(s.noncep2)
    , compactFee(s.fee)
    , toAddr(t.toAddr)
    , amount(t.amount)
    , signature(s.signature)
{
}

WartTransferMessage::WartTransferMessage(ReaderCheck<bytesize> r)
    : txid(r.r)
    , reserved(r.r.view<3>())
    , compactFee(CompactUInt::from_value_throw(r.r.uint16()))
    , toAddr(r.r.view<AddressView>())
    , amount(Wart::from_value_throw(r.r.uint64()))
    , signature(r.r.view<65>())
{
    r.assert_read_bytes();
}

Writer& operator<<(Writer& w, TokenTransferMessage m)
{
    return w << m.txid
             << m.reserved
             << m.compactFee
             << m.tokenHash
             << m.toAddr
             << m.amount
             << m.signature;
}

Address TokenTransferMessage::from_address(HashView txHash) const
{
    return signature.recover_pubkey(txHash.data()).address();
}

TxHash TokenTransferMessage::txhash(HashView pinHash) const
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

TokenTransferMessage::TokenTransferMessage(TokenTransferView t, Hash tokenHash, PinHeight ph, AddressView toAddr)
    : txid(t.txid(ph))
    , reserved(t.pin_nonce().reserved)
    , compactFee(t.compact_fee_throw())
    , tokenHash(std::move(tokenHash))
    , toAddr(toAddr)
    , amount(t.amount_throw())
    , signature(t.signature())
{
}


TokenTransferMessage::TokenTransferMessage(const TransactionId& txid, const mempool::entry::Shared& s, const mempool::entry::TokenTransfer& v)
    : txid(txid)
    , reserved(s.noncep2)
    , compactFee(s.fee)
    , tokenHash(v.tokenHash)
    , toAddr(v.toAddr)
    , amount(v.amount)
    , signature(s.signature)
{
}

TokenTransferMessage::TokenTransferMessage(ReaderCheck<bytesize> r)
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
