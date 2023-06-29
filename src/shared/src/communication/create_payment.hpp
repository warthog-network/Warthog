#pragma once

#include "block/body/fee.hpp"
#include "block/body/nonce.hpp"
#include "block/chain/height.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"

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
    bool valid_signature(HashView pinHash, AddressView fromAddress);
    PaymentCreateMessage(PinHeight pinHeight, const Hash& pinHash, const PrivKey&, CompactFee feeCompactHost, const Address& toAddress, Funds amount, NonceId);
    Address from_address(HashView txHash) const;
    TxHash tx_hash(HashView pinHash) const;
    friend Writer& operator<<(Writer&, const PaymentCreateMessage&);
    operator std::vector<uint8_t>();

    // Data members
    PinHeight pinHeight;
    NonceId nonceId;
    NonceReserved reserved;
    CompactFee compactFee;
    Address toAddr;
    Funds amount;
    RecoverableSignature signature;
};
