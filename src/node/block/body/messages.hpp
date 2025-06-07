#pragma once
#include "block/body/elements.hpp"
#include "block/body/nonce.hpp"
#include "block/body/transaction_id.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
#include "tools/variant.hpp"

struct MsgBase {
public:
    MsgBase(TransactionId txid, NonceReserved reserved, CompactUInt compactFee)
        : _txid(std::move(txid))
        , _reserved(std::move(reserved))
        , compactFee(std::move(compactFee))
    {
    }
    MsgBase(Reader& r)
        : _txid { r }
        , _reserved { r }
        , compactFee { r }
    {
    }
    [[nodiscard]] auto& txid() const { return _txid; }
    [[nodiscard]] Wart fee() const { return compactFee.uncompact(); }
    [[nodiscard]] auto compact_fee() const { return compactFee; }
    [[nodiscard]] auto reserved() const { return _reserved; }
    [[nodiscard]] AccountId from_id() const { return _txid.accountId; }
    [[nodiscard]] PinHeight pin_height() const { return txid().pinHeight; }
    [[nodiscard]] NonceId nonce_id() const { return txid().nonceId; }

private:
    TransactionId _txid;
    NonceReserved _reserved;
    CompactUInt compactFee;
};

template <typename... Ts>
class CreatedTransactionMsg;

template <typename... Ts>
class TransactionMsg : public MsgBase {

protected:
    using parent = TransactionMsg;

public:
    TransactionMsg(const TransactionId& txid, NonceReserved reserved, CompactUInt compactFee, Ts... ts, RecoverableSignature signature)
        : MsgBase { txid, std::move(reserved), std::move(compactFee) }
        , data(std::move(ts)...)
        , _signature(signature)
    {
    }
    TransactionMsg(CreatedTransactionMsg<Ts...>);
    TransactionMsg(Reader& r)
        : MsgBase { r }
        , data({ Ts(r)... })
        , _signature(r)
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

        return std::apply([&](const auto&... arg) {
            return TxHash(((HasherSHA256()
                               << pinHash
                               << txid().pinHeight
                               << txid().nonceId
                               << reserved()
                               << compact_fee().uncompact())
                << ... << arg));
        },
            data);
    };
    [[nodiscard]] Wart spend_wart_throw() const { return fee(); } // default only spend fee, but is overridden in WartTransferMessage
    [[nodiscard]] Address from_address(const TxHash& txHash) const
    {
        return _signature.recover_pubkey(txHash.data()).address();
    }
    template <size_t i>
    auto& get() const
    {
        return std::get<i>(data);
    }
    auto& signature() const { return _signature; }

protected:
    std::tuple<Ts...> data;
    RecoverableSignature _signature;
};

template <typename... Ts>
class CreatedTransactionMsg : public TransactionMsg<Ts...> {
};
template <typename... Ts>
TransactionMsg<Ts...>::TransactionMsg(CreatedTransactionMsg<Ts...> m)
    : TransactionMsg<Ts...>(std::move(*(TransactionMsg<Ts...>*)(&m)))
{
}

class WartTransferMessage : public TransactionMsg<Address, Wart> {
public:
    using WartTransfer = block::body::WartTransfer;
    using TransactionMsg<Address, Wart>::TransactionMsg;

    [[nodiscard]] const auto& to_address() const { return get<0>(); }
    [[nodiscard]] const auto& amount() const { return get<1>(); }
    [[nodiscard]] Wart spend_wart_throw() const { return Wart::sum_throw(fee(), amount()); }
};

class TokenTransferMessage : public TransactionMsg<TokenHash, Address, Funds_uint64> { // for defi we include the token hash
public:
    using TransactionMsg::TransactionMsg;
    using TokenTransfer = block::body::TokenTransfer;
    // layout:
    // TokenTransferMessage(const TransactionId& txid, const mempool::entry::Shared& s, const mempool::entry::TokenTransfer& v);
    // TokenTransferMessage(TokenTransferView, Hash tokenHash, PinHeight, AddressView toAddr);

    [[nodiscard]] const auto& token_hash() const { return get<0>(); }
    [[nodiscard]] const auto& address() const { return get<1>(); }
    [[nodiscard]] const auto& amount() const { return get<2>(); }
};
// class CreateOrderMessage2: public TransactionMessage<> {
//     RecoverableSignature signature;
// };
class CancelMessage {
};
class AddLiquidityMessage {
};
class RemoveLiquidityMessage {
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
    [[nodiscard]] auto reserved() const { return base().reserved(); }
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
