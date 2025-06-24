#pragma once

#include "block/body/messages.hpp"
#include "block/body/nonce.hpp"
#include "block/chain/height.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/base_elements.hpp"
#include "general/compact_uint.hpp"

template <typename Derived, typename... Ts>
class TransactionCreate : public CombineElements<PinHeightEl, NonceIdEl, NonceReservedEl, CompactFeeEl, Ts..., SignatureEl> {

public:
    using CombineElements<PinHeightEl, NonceIdEl, NonceReservedEl, CompactFeeEl, Ts..., SignatureEl>::CombineElements;
    [[nodiscard]] auto create_message(AccountId aid) const
    {
        TransactionId txid(aid, this->pin_height(), this->nonce_id());
        return typename Derived::message_t { txid, this->nonce_reserved(), this->compact_fee(), static_cast<const Ts*>(this)->get()..., this->signature() };
    }
    [[nodiscard]] TxHash tx_hash(const PinHash& pinHash) const
    {
        return TxHash(((HasherSHA256()
                           << pinHash
                           << this->pin_height()
                           << this->nonce_id()
                           << this->nonce_reserved()
                           << this->compact_fee().uncompact())
            << ... << static_cast<Ts*>(this)->get()));
    }
    [[nodiscard]] Address from_address(const TxHash& txHash) const
    {
        return this->signature().recover_pubkey(txHash.data()).address();
    }
    [[nodiscard]] bool valid_signature(const PinHash& pinHash, AddressView fromAddress) const
    {
        return from_address(tx_hash(pinHash)).address() == fromAddress;
    }
    TransactionCreate(PinHeight pinHeight, NonceId nonceId, CompactUInt compactFee, Ts... ts, const Hash& pinHash, const PrivKey& pk, NonceReserved reserved = NonceReserved::zero())
        : TransactionCreate(std::move(pinHeight), std::move(nonceId), std::move(reserved), std::move(compactFee), std::move(ts)..., pk.sign(tx_hash(pinHash)))
    {
    }
};

class WartTransferCreate : public TransactionCreate<WartTransferCreate, ToAddrEl, WartEl> {
public:
    using message_t = WartTransferMessage;
    using TransactionCreate::TransactionCreate;
    operator std::string();
};

class TokenTransferCreate : public TransactionCreate<TokenTransferCreate, ToAddrEl, AmountEl, AssetHashEl> {
public:
    using message_t = TokenTransferMessage;
    using TransactionCreate::TransactionCreate;
};
