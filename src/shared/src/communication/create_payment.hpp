#pragma once

#include "block/body/nonce.hpp"
#include "block/chain/height.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/compact_uint.hpp"
#include "mempool/entry.hpp"

template <typename... Ts>
class TransactionCreate {

public:
    static constexpr size_t byte_size() { return PinHeight::byte_size() + NonceId::byte_size() + NonceReserved::byte_size() + CompactUInt::byte_size() + (Ts::byte_size() + ...) + RecoverableSignature::byte_size(); };
    [[nodiscard]] auto pin_height() const { return pinHeight; }
    [[nodiscard]] auto nonce_id() const { return nonceId; }
    [[nodiscard]] auto nonce_reserved() const { return reserved; }
    [[nodiscard]] auto fee() const { return compactFee.uncompact(); }
    [[nodiscard]] auto& signature() const { return _signature; }
    template <size_t i>
    requires(i < sizeof...(Ts))
    auto& get() const
    {
        return std::get<i>(data);
    }

    [[nodiscard]] std::pair<TransactionId, mempool::entry::Shared> mempool_data(AccountId accountId, TransactionHeight txHeight, Hash txHash) const
    {

        return {
            { accountId, pinHeight, nonceId },
            {
                .transactionHeight { txHeight },
                .nonceReserved { reserved },
                .fee { compactFee },
                .txHash { txHash },
                .signature { _signature },
            }
        };
    };
    bool valid_signature(HashView pinHash, AddressView fromAddress) const
    {
        return _signature.recover_pubkey(tx_hash(pinHash)).address() == fromAddress;
    }
    Address from_address(
        HashView txHash) const
    {
        return _signature.recover_pubkey(txHash.data()).address();
    }
    TransactionCreate(PinHeight pinHeight, NonceId nonceId, NonceReserved reserved, CompactUInt compactFee, Ts... ts, RecoverableSignature rsignature)
        : pinHeight(std::move(pinHeight))
        , nonceId(nonceId)
        , reserved(std::move(reserved))
        , compactFee(std::move(compactFee))
        , data(std::move(ts)...)
        , _signature(std::move(rsignature))
    {
    }
    TransactionCreate(PinHeight pinHeight, NonceId nonceId, CompactUInt compactFee, Ts... ts, const Hash& pinHash, const PrivKey& pk, NonceReserved reserved = NonceReserved::zero())
        : pinHeight(std::move(pinHeight))
        , nonceId(nonceId)
        , reserved(std::move(reserved))
        , compactFee(std::move(compactFee))
        , data(std::move(ts)...)
        , _signature(pk.sign(tx_hash(pinHash)))
    {
    }

    TxHash tx_hash(HashView pinHash) const
    {
        return std::apply([&](const auto&... arg) {
            return TxHash(((HasherSHA256()
                               << pinHash
                               << pinHeight
                               << nonceId
                               << reserved
                               << compactFee.uncompact())
                << ... << arg));
        },
            data);
    }

protected:
    PinHeight pinHeight;
    NonceId nonceId;
    NonceReserved reserved;
    CompactUInt compactFee;
    std::tuple<Ts...> data;
    RecoverableSignature _signature;
};

class WartTransferCreate : public TransactionCreate<Address, Wart> {
public:
    using TransactionCreate::TransactionCreate;
    operator std::string();
    auto& to_addr() const { return get<0>(); }
    auto& wart() const { return get<1>(); }
};

class TokenTransferCreate : public TransactionCreate<Address, Funds_uint64, Hash> {
public:
    using TransactionCreate::TransactionCreate;
    auto& to_addr() const { return get<0>(); }
    auto& amount() const { return get<1>(); }
    auto& hash() const { return get<2>(); }
};
