#pragma once

#include "block/body/nonce.hpp"
#include "block/chain/height.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"
#include "general/compact_uint.hpp"

class PaymentCreateMessage {
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
    PaymentCreateMessage(ReaderCheck<bytesize> r);
    PaymentCreateMessage(PinHeight pinHeight, const Hash& pinHash, const PrivKey&, CompactUInt feeCompactHost, const Address& toAddress, Funds amount, NonceId);
    PaymentCreateMessage(PinHeight pinHeight, NonceId nonceId, NonceReserved reserved, CompactUInt compactFee, Address toAddr, Funds amount, RecoverableSignature signature)
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
    friend Writer& operator<<(Writer&, const PaymentCreateMessage&);
    operator std::vector<uint8_t>();
    operator std::string();

    // Data members
    PinHeight pinHeight;
    NonceId nonceId;
    NonceReserved reserved;
    CompactUInt compactFee;
    Address toAddr;
    Funds amount;
    RecoverableSignature signature;
};
