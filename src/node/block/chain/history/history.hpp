#pragma once
#include "block/body/transaction_id.hpp"
#include "chainserver/state/block_apply/types.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hash.hpp"
#include "defi/token/token.hpp"
#include "defi/uint64/pool.hpp"
#include "defi/uint64/types.hpp"
#include <strings.h>
#include <unistd.h>
class Headerchain;

class TxIdVerifier;

namespace history {
template <uint8_t I, typename... Ts>
requires(I < 256)
struct IdCombine : public CombineElements<Ts...> {
    using CombineElements<Ts...>::CombineElements;
    constexpr static uint8_t indicator = I;
};

template <uint8_t I, typename... Ts>
requires(I < 256)
using IdCombineSigned = IdCombine<I, PinNonceEl, CompactFeeEl, Ts...>;

using WartTransferData = IdCombineSigned<1, OriginAccIdEl, ToAccIdEl, WartEl>;
using RewardData = IdCombine<2, ToAccIdEl, WartEl>;
using AssetCreationData = IdCombineSigned<3, AssetIdEl, OwnerIdEl, AssetSupplyEl, AssetNameEl>;
using TokenTransferData = IdCombineSigned<4, TokenIdEl, OriginAccIdEl, ToAccIdEl, AmountEl>;
using OrderData = IdCombineSigned<5, AssetIdEl, BuyEl, AccountIdEl, LimitPriceEl, AmountEl>;
using CancelationData = IdCombineSigned<6, CancelTxidEl>;

struct PoolBeforeEl : public ElementBase<defi::BaseQuote> {
    using base::base;
    [[nodiscard]] const auto& pool_before() const { return data; }
};

struct PoolAfterEl : public ElementBase<defi::BaseQuote> {
    using base::base;
    [[nodiscard]] const auto& pool_after() const { return data; }
};

using Swap = CombineElements<OrderIdEl, defi::BaseQuote>;

template <typename T>
struct vect_len32_base : public std::vector<T> {
public:
    using std::vector<T>::vector;
    vect_len32_base(Reader& r)
    {
        size_t N { r.uint32() };
        this->reserve(N);
        for (size_t i { 0 }; i < N; ++i) {
            this->push_back(T { r });
        }
    }
    friend Writer& operator<<(Writer& w, const vect_len32_base<T>& v)
    {
        w << uint32_t(v.size());
        for (auto& e : v)
            w << e;
        return w;
    }
};

template <typename T>
struct vect_len32 : public vect_len32_base<T> {
    using vect_len32_base<T>::vect_len32_base;
    [[nodiscard]] size_t byte_size() const
    {
        size_t N { 4 };
        for (auto& e : *this) {
            N += e.byte_size();
        }
        return N;
    }
};
template <HasStaticBytesize T>
struct vect_len32<T> : public vect_len32_base<T> {
    using vect_len32_base<T>::vect_len32_base;
    [[nodiscard]] size_t byte_size() const
    {
        return 4 + this->size() * T::byte_size();
    }
};

struct BuySwapsEl : public ElementBase<vect_len32<CombineElements<BaseEl, QuoteEl, ReferredHistoryIdEl>>> {
    using base::base;
    [[nodiscard]] const auto& buy_swaps() const { return data; }
    [[nodiscard]] auto& buy_swaps() { return data; }
};

struct SellSwapsEl : public ElementBase<vect_len32<CombineElements<BaseEl, QuoteEl, ReferredHistoryIdEl>>> {
    using base::base;
    [[nodiscard]] const auto& sell_swaps() const { return data; }
    [[nodiscard]] auto& sell_swaps() { return data; }
};

struct MatchData : public IdCombine<7, AssetIdEl, PoolBeforeEl, PoolAfterEl, BuySwapsEl, SellSwapsEl> {
    MatchData(AssetId assetId, defi::PoolLiquidity_uint64 poolBefore, defi::PoolLiquidity_uint64 poolAfter)
        : MatchData(assetId, defi::BaseQuote_uint64(std::move(poolBefore)), std::move(poolAfter), {}, {})
    {
    }
    using IdCombine::IdCombine;
};

using LiquidityDeposit = IdCombineSigned<8, BaseEl, QuoteEl, SharesEl, AssetIdEl>;
using LiquidityWithdraw = IdCombineSigned<9, BaseEl, QuoteEl, SharesEl, AssetIdEl>;

template <typename gen_parse_exception, typename... Ts>
struct IndicatorVariant : public wrt::variant<Ts...> {
private:
    [[nodiscard]] static IndicatorVariant parse(Reader& r)
    {
        std::optional<IndicatorVariant> o;
        auto i { r.uint8() };
        if (([&] {
                if (i == Ts::indicator) {
                    o.template emplace<Ts>(r);
                    return true;
                }
                return false;
            }() || ...))
            return std::move(*o);
        throw gen_parse_exception();
    }
    [[nodiscard]] static IndicatorVariant parse(std::vector<uint8_t> v)
    {
        Reader r(v);
        auto p { parse(r) };
        if (!r.eof())
            throw Error(EMSGINTEGRITY);
        return p;
    }

public:
    using wrt::variant<Ts...>::variant;
    IndicatorVariant(wrt::variant<Ts...> v)
        : wrt::variant<Ts...>(std::move(v))
    {
    }
    IndicatorVariant(const std::vector<uint8_t>& v)
        : IndicatorVariant(parse(v))
    {
    }
    size_t byte_size() const
    {
        return 1 + this->visit([](auto& t) { return t.byte_size(); });
    }
    friend Writer& operator<<(Writer& w, const IndicatorVariant& f)
    {
        f.visit([&](auto& v) {
            w << v.indicator << v;
        });
        return w;
    }
    std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> out;
        out.resize(byte_size());
        Writer w(out);
        w << *this;
        assert(w.remaining() == 0);
        return out;
    }
};

struct CantParseHistoryExceptionGenerator {
    std::exception operator()() const
    {
        return std::runtime_error("Cannot parse history entry");
    }
};
using HistoryVariant = IndicatorVariant<CantParseHistoryExceptionGenerator, WartTransferData, RewardData, AssetCreationData, TokenTransferData, OrderData, CancelationData, MatchData, LiquidityDeposit, LiquidityWithdraw>;

struct Entry {
    Entry(const RewardInternal& p);
    Entry(const VerifiedWartTransfer& p);
    Entry(const VerifiedTokenTransfer& p, TokenId);
    Entry(const VerifiedOrder& p);
    Entry(const VerifiedCancelation& p);
    Entry(const VerifiedAssetCreation& p, AssetId);
    Entry(const VerifiedLiquidityDeposit& p, Funds_uint64 receivedShares, AssetId assetId);
    Entry(const VerifiedLiquidityWithdrawal& p, Funds_uint64 receivedBase, Wart receivedQuote, AssetId assetId);
    Entry(Hash h, MatchData);
    Entry(Hash h, HistoryVariant data)
        : hash(std::move(h))
        , data(std::move(data))
    {
    }
    TxHash hash;
    HistoryVariant data;
};

}
