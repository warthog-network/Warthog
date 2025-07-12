#pragma once

#include "block/body/transaction_id.hpp"
#include "block/chain/history/index.hpp"
#include "crypto/address.hpp"
#include "crypto/hash.hpp"
#include "defi/token/token.hpp"
#include "defi/types.hpp"
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
    Wart wart;
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

// struct OrderInternal;
// struct VerifiedOrder : public VerifiedTransaction {
//     VerifiedOrder(const OrderInternal& o, const TransactionVerifier&, HashView tokenHash);
//     const OrderInternal& order;
// };
//
// struct OrderInternal : public SignerData {
//     Price_uint64 limit;
//     Funds_uint64 amount;
//     AssetId assetId;
//     bool buy;
//     [[nodiscard]] VerifiedOrder verify(const TransactionVerifier& tv, HashView tokenHash) const
//     {
//         return { *this, tv, tokenHash };
//     }
// };

namespace block_apply {
template <typename Combined, typename VerifyArgPack>
struct Internal;

template <typename internal_t>
struct Verified : public VerifiedTransaction {
    Verified(const internal_t& internal, const TransactionVerifier& verifier)
        : VerifiedTransaction(verifier.verify(internal, internal))
        , ref(internal) {

        };
    const internal_t& ref;
};

template <typename... T>
struct ArgTypesPack {
};

template <typename Combined, typename... VerifyArgs>
struct Internal<Combined, ArgTypesPack<VerifyArgs...>> : public SignerData, public Combined {
    Internal(SignerData s, Combined e)
        : SignerData(std::move(s))
        , Combined(std::move(e))
    {
    }
    Verified<Combined> verify(const TransactionVerifier& verifier, const VerifyArgs&... args) const
    {
        verify_tuple(verifier, std::forward_as_tuple(args...));
    }

private:
    Verified<Combined> verify_tuple(const TransactionVerifier& verifier, std::tuple<const VerifyArgs&...>) const
    {
        return Verified<Combined> { *this, verifier };
    }
};

namespace selectors {
template <bool replace, size_t I>
struct SelectorElement { };
template <typename... SelectorElements>
struct Selectors {
    template <typename T>
    using append = Selectors<SelectorElements..., T>;
};
template <typename Selectors, typename RemainingPath, size_t I, size_t J>
struct SelectorComputer;
template <bool... bs>
struct SelectorPath {
    using Selectors = SelectorComputer<Selectors<>, SelectorPath<bs...>, 0, 0>::selectors;
};

// compute path using recursion
template <typename Selectors, typename RemainingPath, size_t I, size_t J>
struct SelectorComputer {
    template <typename Path>
    struct Recurse;

    template <bool... remainder>
    struct Recurse<SelectorPath<true, remainder...>> {
        using next = SelectorComputer<typename Selectors::template append<SelectorElement<true, J>>, SelectorPath<remainder...>, I + 1, J + 1>;
    };
    template <bool... remainder>
    struct Recurse<SelectorPath<false, remainder...>> {
        using next = SelectorComputer<typename Selectors::template append<SelectorElement<false, I>>, SelectorPath<remainder...>, I + 1, J>;
    };
    using selectors = Recurse<RemainingPath>::next::selectors;
};
// stopping condition for recursion
template <typename Selectors, size_t I, size_t J>
struct SelectorComputer<Selectors, SelectorPath<>, I, J> {
    using selectors = Selectors;
};

// Now define the templated selector generator
template <bool... bs>
using GetSel = SelectorPath<bs...>::Selectors;

// Checking for sanity
using Test1 = GetSel<false, true, false, true>;
using Test2 = Selectors<SelectorElement<false, 0>, SelectorElement<true, 0>, SelectorElement<false, 2>, SelectorElement<true, 1>>;
static_assert(std::is_same_v<Test1, Test2>);
}


template <typename... Elements>
struct make_type {
private:
    using Combined = CombineElements<Elements...>;

public:
    using Internal = Internal<Combined, ArgTypesPack<>>;
    using Verified = Verified<Internal>;
};

struct CancelTxIdElement : public ElementBase<TransactionId> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& cancel_txid() const { return data; }
};

using Cancelation = make_type<CancelTxIdElement>;

void function_name(Cancelation::Verified& c)
{
    c.hash;
}
// struct Cancelation : public CombineElements<CancelTxIdElement> {
//     using CombineElements::CombineElements;
// };
// using CancelationInternal = Internal<Cancelation>;
// using CancelationVerified = Verified<Cancelation>;

struct BaseQuoteEl : public ElementBase<defi::BaseQuote> {
    using ElementBase::ElementBase;
    [[nodiscard]] const auto& base_quote() const { return data; }
    [[nodiscard]] const auto& base() const { return data.base(); }
    [[nodiscard]] const auto& quote() const { return data.quote(); }
};
struct LiquidityDeposit : public CombineElements<BaseQuoteEl> {
    using CombineElements::CombineElements;
};
using LiquidityDepositInternal = Internal<LiquidityDeposit>;
using LiquidityDepositVerified = Verified<LiquidityDeposit>;

struct LiquidityWithdrawal : public CombineElements<AmountEl> {
    using CombineElements::CombineElements;
};
using LiquidityWithdrawalInternal = Internal<LiquidityWithdrawal>;
using LiquidityWithdrawalVerified = Verified<LiquidityWithdrawal>;

struct WartTransfer : public CombineElements<ToAccIdEl, WartEl> {
    using CombineElements::CombineElements;
};
using WartTransferInternal = Internal<WartTransfer>;
using WartTransferVerified = Verified<WartTransfer>;

struct TokenTransfer : public CombineElements<ToAccIdEl, AmountEl> {
    using CombineElements::CombineElements;
};
using TokenTransferInternal = Internal<TokenTransfer>;
using TokenTransferVerified = Verified<TokenTransfer>;

struct Order : public CombineElements<LimitPriceEl, AmountEl, AssetIdEl, BuyEl> {
    using CombineElements::CombineElements;
};
using OrderInternal = Internal<Order>;
template <>
struct
    using OrderVerified
    = Verified<Order>;

struct OrderInternal;
struct VerifiedOrder : public VerifiedTransaction {
    VerifiedOrder(const OrderInternal& o, const TransactionVerifier&, HashView tokenHash);
    const OrderInternal& order;
};

struct OrderInternal : public SignerData {
    Price_uint64 limit;
    Funds_uint64 amount;
    AssetId assetId;
    bool buy;
    [[nodiscard]] VerifiedOrder verify(const TransactionVerifier& tv, HashView tokenHash) const
    {
        return { *this, tv, tokenHash };
    }
};

}

// struct WartTransferInternal;
// class VerifiedWartTransfer : public VerifiedTransaction {
//     friend struct WartTransferInternal;
//     VerifiedWartTransfer(const WartTransferInternal&, const TransactionVerifier&); // Wart transfer
//
// public:
//     const WartTransferInternal& ti;
// };
//
// struct WartTransferInternal : public TransferInternalWithoutAmount {
//     Wart amount;
//     WartTransferInternal(TransferInternalWithoutAmount t, Wart amount)
//         : TransferInternalWithoutAmount(std::move(t))
//         , amount(amount)
//     {
//     }
//     using TransferInternalWithoutAmount::TransferInternalWithoutAmount;
//     [[nodiscard]] VerifiedWartTransfer verify(const TransactionVerifier& tv) const
//     {
//         return { *this, tv };
//     }
// };

// struct TokenTransferInternal;
// class VerifiedTokenTransfer : public VerifiedTransaction {
//     friend struct TokenTransferInternal;
//     VerifiedTokenTransfer(const TokenTransferInternal&, const TransactionVerifier&, HashView tokenHash);
//
// public:
//     const TokenTransferInternal& ti;
// };
// struct TokenTransferInternal : public TransferInternalWithoutAmount {
//     Funds_uint64 amount;
//     TokenTransferInternal(TransferInternalWithoutAmount t, Funds_uint64 amount)
//         : TransferInternalWithoutAmount(std::move(t))
//         , amount(amount)
//     {
//     }
//     [[nodiscard]] VerifiedTokenTransfer verify(const TransactionVerifier& tv, HashView tokenHash) const
//     {
//         return { *this, tv, tokenHash };
//     }
// };

struct TokenCreationInternal;
struct VerifiedAssetCreation : public VerifiedTransaction {
    friend struct TokenCreationInternal;
    VerifiedAssetCreation(const TokenCreationInternal&, const TransactionVerifier&);
    const TokenCreationInternal& tci;
};

struct TokenCreationInternal : public SignerData {
    size_t index;
    AssetName name;
    FundsDecimal supply;
    [[nodiscard]] VerifiedAssetCreation verify(const TransactionVerifier& tv) const
    {
        return { *this, tv };
    }
};
