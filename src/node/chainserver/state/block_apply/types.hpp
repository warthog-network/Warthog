#pragma once

#include "block/body/transaction_id.hpp"
#include "block/chain/history/index.hpp"
#include "crypto/address.hpp"
#include "crypto/hash.hpp"
#include "defi/token/token.hpp"
#include "defi/uint64/price.hpp"
#include "signature_verification.hpp"
struct SwapInternal {
    HistoryId oId;
    TransactionId txid;
    Funds_uint64 base;
    Wart quote;
};

struct BuySwapInternal : public SwapInternal {
};
struct SellSwapInternal : public SwapInternal {
};

struct RewardInternal {
    ValidAccountId toAccountId;
    Wart amount;
    NonzeroHeight height;
    AddressView toAddress;
    Hash hash() const;
};

// struct ProcessedMatch : public history::MatchData {
//     Hash hash;
//
// protected:
//     ProcessedMatch(const Hash& blockHash, TokenId tokenId);
// };

struct TransferInternalWithoutAmount : public SignerData {
    IdAddressView to;
};

struct OrderInternal;
struct VerifiedOrder : public VerifiedTransaction {
    VerifiedOrder(const OrderInternal& o, const TransactionVerifier&, HashView tokenHash);
    const OrderInternal& order;
};
struct OrderInternal : public SignerData {
    Price_uint64 limit;
    TokenFunds amount;
    bool buy;
    [[nodiscard]] VerifiedOrder verify(const TransactionVerifier& tv, HashView tokenHash) const
    {
        return { *this, tv, tokenHash };
    }
};

struct CancelationInternal;
struct VerifiedCancelation : public VerifiedTransaction {
    VerifiedCancelation(const CancelationInternal&, const TransactionVerifier&);
    const CancelationInternal& cancelation;
};

struct CancelationInternal : public SignerData {
    TransactionId cancelTxid;
    [[nodiscard]] VerifiedCancelation verify(const TransactionVerifier& tv) const
    {
        return { *this, tv };
    }
};

struct LiquidityAddInternal : public SignerData {
};

struct LiquidityRemoveInternal : public SignerData {
};

struct WartTransferInternal;
class VerifiedWartTransfer : public VerifiedTransaction {
    friend struct WartTransferInternal;
    VerifiedWartTransfer(const WartTransferInternal&, const TransactionVerifier&); // Wart transfer

public:
    const WartTransferInternal& ti;
};

struct WartTransferInternal : public TransferInternalWithoutAmount {
    Wart amount;
    WartTransferInternal(TransferInternalWithoutAmount t, Wart amount)
        : TransferInternalWithoutAmount(std::move(t))
        , amount(amount)
    {
    }
    using TransferInternalWithoutAmount::TransferInternalWithoutAmount;
    [[nodiscard]] VerifiedWartTransfer verify(const TransactionVerifier& tv) const
    {
        return { *this, tv };
    }
};

struct TokenTransferInternal;
class VerifiedTokenTransfer : public VerifiedTransaction {
    friend struct TokenTransferInternal;
    VerifiedTokenTransfer(const TokenTransferInternal&, const TransactionVerifier&, HashView tokenHash);

public:
    const TokenTransferInternal& ti;
};
struct TokenTransferInternal : public TransferInternalWithoutAmount {
    Funds_uint64 amount;
    TokenTransferInternal(TransferInternalWithoutAmount t, Funds_uint64 amount)
        : TransferInternalWithoutAmount(std::move(t))
        , amount(amount)
    {
    }
    [[nodiscard]] VerifiedTokenTransfer verify(const TransactionVerifier& tv, HashView tokenHash) const
    {
        return { *this, tv, tokenHash };
    }
};

struct TokenCreationInternal;
struct VerifiedTokenCreation : public VerifiedTransaction {
    friend struct TokenCreationInternal;
    VerifiedTokenCreation(const TokenCreationInternal&, const TransactionVerifier&);
    const TokenCreationInternal& tci;
};

struct TokenCreationInternal : public SignerData {
    size_t index;
    TokenName name;
    FundsDecimal supply;
    [[nodiscard]] VerifiedTokenCreation verify(const TransactionVerifier& tv) const
    {
        return { *this, tv };
    }
};

