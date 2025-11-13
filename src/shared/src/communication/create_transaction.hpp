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
class TransactionCreateBase : public CombineElements<PinHeightEl, NonceIdEl, NonceReservedEl, CompactFeeEl, Ts..., SignatureEl> {

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
            << ... << static_cast<const Ts*>(this)->get()));
    }
    [[nodiscard]] Address from_address(const TxHash& txHash) const
    {
        return this->signature().recover_pubkey(txHash.data()).address();
    }
    [[nodiscard]] bool valid_signature(const PinHash& pinHash, AddressView fromAddress) const
    {
        return from_address(tx_hash(pinHash)) == fromAddress;
    }
    TransactionCreateBase(PinHeight pinHeight, NonceId nonceId, CompactUInt compactFee, Ts... ts, const Hash& pinHash, const PrivKey& pk, NonceReserved reserved = NonceReserved::zero())
        : TransactionCreateBase(std::move(pinHeight), std::move(nonceId), std::move(reserved), std::move(compactFee), std::move(ts)..., pk.sign(tx_hash(pinHash)))
    {
    }
};

#define DEFINE_CREATE_MESSAGE(name, str_tag, ...)                  \
    class name : public TransactionCreateBase<name, __VA_ARGS__> { \
    public:                                                        \
        const char* tag() const { return str_tag; };               \
        using TransactionCreateBase::TransactionCreateBase;        \
        operator std::string();                                    \
    };

DEFINE_CREATE_MESSAGE(WartTransferCreate, "WartTransfer", ToAddrEl, NonzeroWartEl)
DEFINE_CREATE_MESSAGE(TokenTransferCreate, "TokenTransfer", AssetHashEl, LiquidityFlagEl, ToAddrEl, NonzeroAmountEl)
DEFINE_CREATE_MESSAGE(OrderCreate, "Order", AssetHashEl, BuyEl, AmountEl, LimitPriceEl)
DEFINE_CREATE_MESSAGE(LiquidityDepositCreate, "LiquidityDeposit", AssetHashEl, BaseEl, QuoteEl)
DEFINE_CREATE_MESSAGE(LiquidityWithdrawalCreate, "LiquidityWithdrawal", AssetHashEl, NonzeroAmountEl)
DEFINE_CREATE_MESSAGE(CancelationCreate, "Cancelation", CancelHeightEl, CancelNonceEl)
DEFINE_CREATE_MESSAGE(AssetCreationCreate, "AssetCreation", AssetSupplyEl, AssetNameEl)

#undef DEFINE_CREATE_MESSAGE

using CreateVariant = wrt::variant<WartTransferCreate, TokenTransferCreate, OrderCreate, LiquidityDepositCreate, LiquidityWithdrawalCreate, CancelationCreate, AssetCreationCreate>;

struct TransactionCreate : CreateVariant {
    using CreateVariant::CreateVariant;
    std::string tag() const
    {
        return visit([&](auto& createTransaction) -> std::string {
            return createTransaction.tag();
        });
    }
};
