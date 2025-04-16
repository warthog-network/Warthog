#pragma once

#include "block/body/nonce.hpp"
#include "block/chain/height.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"
#include "general/compact_uint.hpp"

class WartTransferCreate {
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
    WartTransferCreate(PinHeight pinHeight, const Hash& pinHash, const PrivKey&, CompactUInt feeCompactHost, const Address& toAddress, Wart amount, NonceId);
    WartTransferCreate(PinHeight pinHeight, NonceId nonceId, NonceReserved reserved, CompactUInt compactFee, Address toAddr, Wart amount, RecoverableSignature signature)
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

class TokenTransferCreate {
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
    TokenTransferCreate(ReaderCheck<bytesize> r);
    TokenTransferCreate(PinHeight pinHeight, const Hash& pinHash, const PrivKey&, Hash tokenHash, CompactUInt feeCompactHost, const Address& toAddress, ParsedFunds amount, NonceId);
    TokenTransferCreate(PinHeight pinHeight, NonceId nonceId, NonceReserved reserved, Hash tokenHash, CompactUInt compactFee, Address toAddr, ParsedFunds amount, RecoverableSignature signature)
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
    operator std::string();

    // Data members
    PinHeight pinHeight;
    NonceId nonceId;
    NonceReserved reserved;
    TokenHash tokenHash;
    CompactUInt compactFee;
    Address toAddr;
    ParsedFunds amount;
    RecoverableSignature signature;
};
