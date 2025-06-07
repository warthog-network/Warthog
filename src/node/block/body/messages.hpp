#pragma once
#include "block/body/elements.hpp"
#include "block/body/nonce.hpp"
#include "block/body/transaction_id.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
#include "tools/variant.hpp"

using MsgBase = CombineElements<TransactionIdElement, NonceReservedElement, CompactFeeElement>;

template <typename... Ts>
class CreatedTransactionMsg;

template <typename... Ts>
class TransactionMsg : public MsgBase, public CombineElements<Ts..., SignatureElement> {

protected:
    using parent = TransactionMsg;

public:
    TransactionMsg(const TransactionId& txid, NonceReserved reserved, CompactUInt compactFee, Ts... ts, RecoverableSignature signature)
        : MsgBase { txid, std::move(reserved), std::move(compactFee) }
        , Ts(std::move(ts))...
        , SignatureElement(std::move(signature))
    {
    }
    TransactionMsg(CreatedTransactionMsg<Ts...>);
    TransactionMsg(Reader& r)
        : MsgBase { r }
        , Ts(r)...
        , SignatureElement(r)
    {
    }
    static constexpr size_t byte_size() { return 16 + 3 + 2 + (Ts::byte_size() + ...) + 65; }

    friend Writer& operator<<(Writer& w, TransactionMsg m)
    {
        return w << m.txid()
                 << m.reserved()
                 << m.compact_fee()
                 << (std::get<std::index_sequence_for<Ts>>(m.data) << ...)
                 << m._signature;
    }

    [[nodiscard]] TxHash txhash(const PinHash& pinHash) const
    {

        return TxHash(((HasherSHA256()
                           << pinHash
                           << this->txid().pinHeight
                           << this->txid().nonceId
                           << this->nonce_reserved()
                           << this->compact_fee().uncompact())
            << ... << CombineElements<Ts..., SignatureElement>::template get<Ts>()));
    };
    [[nodiscard]] Wart spend_wart_throw() const { return this->fee(); } // default only spend fee, but is overridden in WartTransferMessage
    [[nodiscard]] Address from_address(const TxHash& txHash) const
    {
        return this->signature().recover_pubkey(txHash.data()).address();
    }
};

template <typename... Ts>
class CreatedTransactionMsg : public TransactionMsg<Ts...> {
};
template <typename... Ts>
TransactionMsg<Ts...>::TransactionMsg(CreatedTransactionMsg<Ts...> m)
    : TransactionMsg<Ts...>(std::move(*(TransactionMsg<Ts...>*)(&m)))
{
}

class WartTransferMessage : public TransactionMsg<ToAddrElement, WartElement> {
public:
    using WartTransfer = block::body::WartTransfer;
    using TransactionMsg::TransactionMsg;

    [[nodiscard]] Wart spend_wart_throw() const { return Wart::sum_throw(fee(), wart()); }
};

class TokenTransferMessage : public TransactionMsg<TokenHashElement, CreatorAddrElement, AmountElement> { // for defi we include the token hash
public:
    using TransactionMsg::TransactionMsg;
};

class OrderMessage : public TransactionMsg<TokenHashElement, BuyElement, AmountElement, LimitPriceElement> { // for defi we include the token hash
public:
    using TransactionMsg::TransactionMsg;
};

class CancelationMessage : public TransactionMsg<CancelPinNonceElement> {
};
class LiquidityAddMessage : public TransactionMsg<TokenHashElement, WartElement, AmountElement> {
};
class LiquidityRemoveMessage : public TransactionMsg<TokenHashElement, AmountElement> {
};
using TransactionVariant = wrt::variant<WartTransferMessage, TokenTransferMessage>;

struct TransactionMessage : public TransactionVariant {
    const MsgBase& base() const
    {
        return *visit([](const auto& m) -> const MsgBase* { return &m; });
    }
    const RecoverableSignature& signature() const
    {
        return *visit([](const auto& m) -> const RecoverableSignature* { return &m.signature(); });
    }
    [[nodiscard]] auto compact_fee() const { return base().compact_fee(); }
    [[nodiscard]] auto spend_wart_throw() const
    {
        return visit([](auto& m) { return m.spend_wart_throw(); });
    }
    [[nodiscard]] auto spend_wart_assert() const
    {
        try {
            return spend_wart_throw();
        } catch (Error e) {
            assert(false);
        }
    }
    [[nodiscard]] auto& txid() const { return base().txid(); }
    [[nodiscard]] auto nonce_reserved() const { return base().nonce_reserved(); }
    [[nodiscard]] AccountId from_id() const { return base().from_id(); }
    [[nodiscard]] PinHeight pin_height() const { return base().pin_height(); }
    [[nodiscard]] NonceId nonce_id() const { return base().nonce_id(); }
    [[nodiscard]] auto from_address(const TxHash txHash) const
    {
        return visit([&](auto& m) { return m.from_address(txHash); });
    }
    [[nodiscard]] TxHash txhash(const PinHash& pinHash) const
    {
        return visit([&](const auto& m) { return m.txhash(pinHash); });
    }
};
