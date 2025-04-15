#pragma once

#include "block/body/nonce.hpp"
#include "block/chain/height.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"
#include "general/compact_uint.hpp"

class WartPaymentCreateMessage {
public:
    static constexpr size_t bytesize { 106 };
    // byte layout:
    // 0 pinHeight
    // 4 nonceId
    // 8 reserved
    // 11 feeCompact
    // 13 toAddress
    // 33 amount
    // 41 signature
    // 106 [total size]
    WartPaymentCreateMessage(ReaderCheck<bytesize> r);
    WartPaymentCreateMessage(PinHeight pinHeight, const Hash& pinHash, const PrivKey&, CompactUInt feeCompactHost, const Address& toAddress, Wart amount, NonceId);
    WartPaymentCreateMessage(PinHeight pinHeight, NonceId nonceId, NonceReserved reserved, CompactUInt compactFee, Address toAddr, Wart amount, RecoverableSignature signature)
        : pinHeight(pinHeight)
        , nonceId(nonceId)
        , reserved(reserved)
        , compactFee(compactFee)
        , toAddr(toAddr)
        , amount(amount)
        , signature(signature)
    {
    }

    bool valid_signature(HashView pinHash, AddressView fromAddress) const;
    Address from_address(HashView txHash) const;
    TxHash tx_hash(HashView pinHash) const;
    friend Writer& operator<<(Writer&, const WartPaymentCreateMessage&);
    operator std::vector<uint8_t>();
    operator std::string();

    // Data members
    PinHeight pinHeight;
    NonceId nonceId;
    NonceReserved reserved;
    CompactUInt compactFee;
    Address toAddr;
    Wart amount;
    RecoverableSignature signature;
};

class TokenPaymentCreateMessage {
public:
    static constexpr size_t bytesize { 138 };
    // byte layout:
    // 0 pinHeight
    // 4 nonceId
    // 8 reserved
    // 11 tokenHash
    // 43 feeCompact
    // 45 toAddress
    // 65 amount
    // 73 signature
    // 138 [total size]
    TokenPaymentCreateMessage(ReaderCheck<bytesize> r);
    TokenPaymentCreateMessage(PinHeight pinHeight, const Hash& pinHash, const PrivKey&, Hash tokenHash, CompactUInt feeCompactHost, const Address& toAddress, Funds_uint64 amount, NonceId);
    TokenPaymentCreateMessage(PinHeight pinHeight, NonceId nonceId, NonceReserved reserved, Hash tokenHash, CompactUInt compactFee, Address toAddr, Funds_uint64 amount, RecoverableSignature signature)
        : pinHeight(pinHeight)
        , nonceId(nonceId)
        , reserved(reserved)
        , tokenHash(std::move(tokenHash))
        , compactFee(compactFee)
        , toAddr(toAddr)
        , amount(amount)
        , signature(signature)
    {
    }

    bool valid_signature(HashView pinHash, AddressView fromAddress) const;
    Address from_address(HashView txHash) const;
    TxHash tx_hash(HashView pinHash) const;
    friend Writer& operator<<(Writer&, const TokenPaymentCreateMessage&);
    operator std::vector<uint8_t>();
    operator std::string();

    // Data members
    PinHeight pinHeight;
    NonceId nonceId;
    NonceReserved reserved;
    TokenHash tokenHash;
    CompactUInt compactFee;
    Address toAddr;
    Funds_uint64 amount;
    RecoverableSignature signature;
};
