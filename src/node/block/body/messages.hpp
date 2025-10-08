#pragma once
#include "block/body/elements.hpp"
#include "block/body/nonce.hpp"
#include "block/body/transaction_id.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/reader.hpp"
#include "tools/indicator_variant.hpp"
#include "tools/variant.hpp"

struct MsgBase : public CombineElements<TransactionIdEl, NonceReservedEl, CompactFeeEl> {
    using CombineElements::CombineElements;
    auto pin_nonce_throw(NonzeroHeight height)
    {
        auto pn { PinNonce::make_pin_nonce(nonce_id(), height, pin_height()) };
        if (!pn)
            throw std::runtime_error("Cannot make pin_nonce");
        return *pn;
    }
};

namespace messages{
struct SpendToken {
    AssetHash hash;
    bool isLiquidity;
    Funds_uint64 amount;
};
}

template <uint8_t indicator, typename... Ts>
class ComposeTransactionMessage : public MsgBase, public CombineElements<Ts..., SignatureEl> {

protected:
    using parent_t = ComposeTransactionMessage;

public:
    static constexpr bool has_asset_hash = std::derived_from<parent_t, AssetHashEl>;
    static constexpr uint8_t INDICATOR { indicator }; // used for serialization of transaction message vectors
    ComposeTransactionMessage(const TransactionId& txid, NonceReserved reserved, CompactUInt compactFee, Ts::data_t... ts, RecoverableSignature signature)
        : MsgBase { txid, std::move(reserved), std::move(compactFee) }
        , CombineElements<Ts..., SignatureEl>(std::move(ts)..., std::move(signature))
    {
    }
    ComposeTransactionMessage(Reader& r)
        : MsgBase { r }
        , CombineElements<Ts..., SignatureEl>(r)
    {
    }
    static constexpr size_t byte_size() { return 16 + 3 + 2 + (Ts::byte_size() + ...) + 65; }

    void serialize(Serializer auto& s) const
    {
        ((s << txid()
            << nonce_reserved()
            << compact_fee())
            << ... << static_cast<const Ts*>(this)->get())
            << SignatureEl::signature();
    }

    [[nodiscard]] TxHash txhash(const PinHash& pinHash) const
    {

        return TxHash(((HasherSHA256()
                           << pinHash
                           << this->txid().pinHeight
                           << this->txid().nonceId
                           << this->nonce_reserved()
                           << this->compact_fee().uncompact())
            << ... << CombineElements<Ts..., SignatureEl>::template get<Ts>()));
    }
    [[nodiscard]] Wart spend_wart_throw() const { return this->fee(); } // default only spend fee, but is overridden in WartTransferMessage and LiquidityDepositMessage
    // [[nodiscard]] std::optional<SpendToken> spend_token_throw() const { return {}; } // default no other token than WART
    [[nodiscard]] Address from_address(const TxHash& txHash) const
    {
        return this->signature().recover_pubkey(txHash.data()).address();
    }
};

class WartTransferMessage : public ComposeTransactionMessage<1, ToAddrEl, WartEl> {
public:
    static_assert(!has_asset_hash);
    using WartTransfer = block::body::WartTransfer;
    WartTransferMessage(TransactionId txid, NonceReserved nr, CompactUInt fee, Address addr, Wart wart, RecoverableSignature sgn)
        : ComposeTransactionMessage(std::move(txid), std::move(nr), std::move(fee), std::move(addr), std::move(wart), std::move(sgn))
    {
        check_throw();
    };
    WartTransferMessage(Reader& r)
        : ComposeTransactionMessage(r)
    {
        check_throw();
    }
    void check_throw()
    {
        if (wart().is_zero())
            throw Error(EZEROWART);
    }

    [[nodiscard]] Wart spend_wart_throw() const { return Wart::sum_throw(fee(), wart()); }
};

class TokenTransferMessage : public ComposeTransactionMessage<2, AssetHashEl, LiquidityFlagEl, ToAddrEl, AmountEl> { // for defi we include the asset hash
public:
    static_assert(has_asset_hash);
    TokenTransferMessage(TransactionId txid, NonceReserved nr, CompactUInt fee, AssetHash ah, bool poolFlag, Address addr, Funds_uint64 amount, RecoverableSignature sgn)
        : ComposeTransactionMessage(std::move(txid), std::move(nr), std::move(fee), std::move(ah), poolFlag, std::move(addr), std::move(amount), std::move(sgn))
    {
        check_throw();
    };
    TokenTransferMessage(Reader& r)
        : ComposeTransactionMessage(r)
    {
        check_throw();
    }
    void check_throw()
    {
        if (amount().is_zero())
            throw Error(EZEROAMOUNT);
    }
    [[nodiscard]] std::optional<messages::SpendToken> spend_token_throw() const { return messages::SpendToken { asset_hash(), is_liquidity(), amount() }; }
};

class AssetCreationMessage : public ComposeTransactionMessage<3, AssetSupplyEl, AssetNameEl> {
public:
    using ComposeTransactionMessage::ComposeTransactionMessage;
};

class OrderMessage : public ComposeTransactionMessage<4, AssetHashEl, BuyEl, AmountEl, LimitPriceEl> { // for defi we include the token hash
    static_assert(has_asset_hash);

public:
    [[nodiscard]] std::optional<messages::SpendToken> spend_token_throw() const
    {
        if (buy())
            return {};
        return messages::SpendToken { asset_hash(), false, amount() };
    }
    [[nodiscard]] Wart spend_wart_throw() const { return Wart::sum_throw(fee(), buy() ? Wart::from_funds_throw(amount()) : Wart(0)); }
    using parent_t::parent_t;
};
class LiquidityDepositMessage : public ComposeTransactionMessage<5, AssetHashEl, WartEl, AmountEl> {
    static_assert(has_asset_hash);

public:
    [[nodiscard]] Wart spend_wart_throw() const { return Wart::sum_throw(fee(), wart()); }
    [[nodiscard]] messages::SpendToken spend_token_throw() const
    {
        return { asset_hash(), false, amount() };
    }
    using parent_t::parent_t;
};

class LiquidityWithdrawMessage : public ComposeTransactionMessage<6, AssetHashEl, AmountEl> {
    static_assert(has_asset_hash);

public:
    [[nodiscard]] messages::SpendToken spend_token_throw() const
    {
        return { asset_hash(), true, amount() };
    }
    using parent_t::parent_t;
};

class CancelationMessage : public ComposeTransactionMessage<7, CancelHeightEl, CancelNonceEl> {
    static_assert(!has_asset_hash);

private:
    void throw_if_bad()
    {
        if (cancel_height() > txid().pinHeight)
            throw Error(ECANCELFUTURE);
        if (cancel_height() == txid().pinHeight && cancel_nonceid() == txid().nonceId)
            throw Error(ECANCELSELF);
    }

public:
    TransactionId cancel_txid() const
    {
        return { from_id(), cancel_height(), cancel_nonceid() };
    }
    CancelationMessage(const TransactionId& txid, NonceReserved reserved, CompactUInt compactFee, PinHeight cancelHeight, NonceId nid, RecoverableSignature signature)
        : ComposeTransactionMessage<7, CancelHeightEl, CancelNonceEl>(txid, reserved, compactFee, cancelHeight, nid, signature)
    {
        throw_if_bad();
    }
    CancelationMessage(Reader& r)
        : ComposeTransactionMessage<7, CancelHeightEl, CancelNonceEl>(r)
    {
        throw_if_bad();
    }
};

struct InvTxTypeExceptionGenerator {
    auto operator()() const
    {
        return Error(ETXTYPE);
    }
};

using TransactionVariant = wrt::indicator_variant<InvTxTypeExceptionGenerator, WartTransferMessage, TokenTransferMessage, AssetCreationMessage, OrderMessage, LiquidityDepositMessage, LiquidityWithdrawMessage, CancelationMessage>;

struct TransactionMessage : public TransactionVariant {
public:
    using TransactionVariant::TransactionVariant;
    const MsgBase& base() const
    {
        return *visit([](const auto& m) -> const MsgBase* { return &m; });
    }
    const RecoverableSignature& signature() const
    {
        return *visit([](const auto& m) -> const RecoverableSignature* { return &m.signature(); });
    }
    [[nodiscard]] auto compact_fee() const { return base().compact_fee(); }
    [[nodiscard]] auto fee() const { return compact_fee().uncompact(); }
    [[nodiscard]] auto spend_wart_throw() const
    {
        return visit([](auto& m) { return m.spend_wart_throw(); });
    }
    [[nodiscard]] auto spend_token_throw() const
    {
        return visit([]<typename T>(const T& m) -> std::optional<messages::SpendToken> {
            if constexpr (T::has_asset_hash) {
                return m.spend_token_throw();
            } else {
                return {};
            }
        });
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
