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
struct ICombine : public CombineElements<Ts...> {
    using CombineElements<Ts...>::CombineElements;
    constexpr static uint8_t indicator = I;
};

using WartTransferData
    = ICombine<1, PinNonceEl, CompactFeeEl, OriginAccIdEl, ToAccIdEl, WartEl>;

using RewardData
    = ICombine<2, ToAccIdEl, WartEl>;

using TokenCreationData
    = ICombine<3, PinNonceEl, CompactFeeEl, TokenIdEl, OwnerIdEl, TokenSupplyEl, TokenNameEl>;

using TokenTransferData
    = ICombine<4, PinNonceEl, CompactFeeEl, TokenIdEl, OriginAccIdEl, ToAccIdEl, AmountEl>;

using OrderData
    = ICombine<5, PinNonceEl, CompactFeeEl, TokenIdEl, BuyEl, AccountIdEl, LimitPriceEl, AmountEl>;

using CancelationData
    = ICombine<6, PinNonceEl, CompactFeeEl, CancelTxidEl>;

struct BaseQuote : public CombineElements<BaseEl, QuoteEl> {
    using CombineElements::CombineElements;
    BaseQuote(const defi::BaseQuote_uint64& b)
        : CombineElements(b.base, Wart::from_funds_throw(b.quote))
    {
    }
};

struct PoolBeforeEl : public ElementBase<BaseQuote> {
    using base::base;
    [[nodiscard]] const auto& pool_before() const { return data; }
};

struct PoolAfterEl : public ElementBase<BaseQuote> {
    using base::base;
    [[nodiscard]] const auto& pool_after() const { return data; }
};

// struct SwapEl : public ElementBase<BaseQuote> {
//     using base::base;
//     [[nodiscard]] const auto& swap() const { return data; }
// };
//
using Swap = CombineElements<OrderIdEl, BaseQuote>;

template <typename T>
struct vect_len32_base : public std::vector<T> {
public:
    using std::vector<T>::vector;
    vect_len32_base(Reader& r)
    {
        size_t N { r.uint32() };
        this->reserve(N);
        for (size_t i { 0 }; i < N; ++i) {
            push_back(T { r });
        }
    }
    friend Writer& operator<<(Writer& w, const vect_len32_base<T>& v)
    {
        w << uint32_t(v.size());
        for (auto& e : v)
            w << e;
        return w;
    }
    [[nodiscard]] size_t byte_size() const
    {
        return _byte_size<T>();
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

struct BuySwapsEl : public ElementBase<vect_len32<BaseQuote>> {
    using base::base;
    [[nodiscard]] const auto& buy_swaps() const { return data; }
    [[nodiscard]] auto& buy_swaps() { return data; }
};
struct SellSwapsEl : public ElementBase<vect_len32<BaseQuote>> {
    using base::base;
    [[nodiscard]] const auto& sell_swaps() const { return data; }
    [[nodiscard]] auto& sell_swaps() { return data; }
};

struct MatchData : public ICombine<7, PoolBeforeEl, PoolAfterEl, BuySwapsEl, SellSwapsEl> {
    MatchData(defi::PoolLiquidity_uint64 poolBefore, defi::PoolLiquidity_uint64 poolAfter)
        : MatchData(defi::BaseQuote_uint64(std::move(poolBefore)), std::move(poolAfter), {}, {})
    {
    }
    using ICombine::ICombine;
};

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
        return 1 + visit([](auto& t) { return t.byte_size(); });
    }
    friend Writer& operator<<(Writer& w, const IndicatorVariant<Ts...>& f)
    {
        f.visit([&](auto& v) {
            w << v.indicator << v;
        });
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

using HistoryVariant = IndicatorVariant<decltype([]() { return std::runtime_error("Cannot parse history entry"); }), WartTransferData, RewardData, TokenCreationData, TokenTransferData, OrderData, CancelationData, MatchData>;

struct Entry {
    Entry(const RewardInternal& p);
    Entry(const VerifiedWartTransfer& p);
    Entry(const VerifiedTokenTransfer& p, TokenId);
    Entry(const VerifiedOrder& p);
    Entry(const VerifiedCancelation& p);
    Entry(const VerifiedTokenCreation& p, TokenId);
    Entry(Hash h, MatchData);
    Entry(Hash h, HistoryVariant data)
        : hash(std::move(h))
        , data(std::move(data))
    {
    }
    Hash hash;
    HistoryVariant data;
};

}
