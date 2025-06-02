#pragma once
#include "block/body/nonce.hpp"
#include "block/body/transaction_id.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
#include "mempool/entry.hpp"

template <typename... Ts>
class TransactionMsg {

protected:
    using parent = TransactionMsg;

public:
    TransactionMsg(const TransactionId& txid, const mempool::entry::Shared& s, Ts... ts)
        : txid(std::move(txid))
        , reserved(std::move(s.noncep2))
        , compactFee(std::move(s.fee))
        , data(std::move(ts)...)
        , signature(s.signature)
    {
    }
    TransactionMsg(Reader& r)
        : txid(r)
        , reserved(r)
        , compactFee(r)
        , data({ Ts(r)... })
        , signature(r)
    {
    }
    static constexpr size_t byte_size() { return 16 + 3 + 2 + (Ts::byte_size() + ...) + 65; }

    friend Writer& operator<<(Writer& w, TransactionMsg m)
    {
        return w << m.txid
                 << m.reserved
                 << m.compactFee
                 << (std::get<std::index_sequence_for<Ts>>(m.data) << ...)
                 << m.signature;
    }

    [[nodiscard]] TxHash txhash(HashView pinHash) const
    {
        return TxHash(HasherSHA256()
            << pinHash
            << txid.pinHeight
            << txid.nonceId
            << reserved
            << compactFee.uncompact()
            << (std::get<std::index_sequence_for<Ts>>(data) << ...));
    };
    [[nodiscard]] Address from_address(HashView txHash) const
    {
        return signature.recover_pubkey(txHash.data()).address();
    }
    template <size_t i>
    auto& get() const
    {
        return std::get<i>(data);
    }

    [[nodiscard]] Wart fee() const { return compactFee.uncompact(); }
    [[nodiscard]] AccountId from_id() const { return txid.accountId; }
    [[nodiscard]] PinHeight pin_height() const { return txid.pinHeight; }
    [[nodiscard]] NonceId nonce_id() const { return txid.nonceId; }

protected:
    TransactionId txid;
    NonceReserved reserved;
    CompactUInt compactFee;
    std::tuple<Ts...> data;
    RecoverableSignature signature;
};

class WartTransferMessage2 : public TransactionMsg<Address, Wart> {
public:
    using WartTransferView = block::body::view::WartTransfer;
    using TransactionMsg<Address, Wart>::TransactionMsg;
    WartTransferMessage2(const TransactionId& txid, const mempool::entry::Shared& s,
        const mempool::entry::WartTransfer& t)
        : parent(txid, s, t.toAddr, t.amount) { };
    WartTransferMessage2(WartTransferView, PinHeight, AddressView toAddr);

    [[nodiscard]] const auto& address() const { return get<0>(); }
    [[nodiscard]] const auto& amount() const { return get<1>(); }
    [[nodiscard]] Funds_uint64 spend_throw() const { return Funds_uint64::sum_throw(fee(), amount()); }
};

class TokenTransferMessage2 : public TransactionMsg<TokenHash, Address, Funds_uint64> { // for defi we include the token hash
public:
    using TransactionMsg::TransactionMsg;
    using TokenTransferView = block::body::view::TokenTransfer;
    // layout:
    TokenTransferMessage2(const TransactionId& txid, const mempool::entry::Shared&, const mempool::entry::TokenTransfer);
    // TokenTransferMessage(const TransactionId& txid, const mempool::entry::Shared& s, const mempool::entry::TokenTransfer& v);
    // TokenTransferMessage(TokenTransferView, Hash tokenHash, PinHeight, AddressView toAddr);

    [[nodiscard]] const auto& token_hash() const { return get<0>(); }
    [[nodiscard]] const auto& address() const { return get<1>(); }
    [[nodiscard]] const auto& amount() const { return get<2>(); }
    [[nodiscard]] Funds_uint64 spend_throw() const { return Funds_uint64::sum_throw(fee(), amount()); }
};
class CreateOrderMessage2: public TransactionMessage<> {
    RecoverableSignature signature;
};
class CancelMessage2 {
};
class AddLiquidityMessage2 {
};
class RemoveLiquidityMessage2 {
};
