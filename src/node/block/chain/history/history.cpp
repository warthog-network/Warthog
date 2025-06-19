#include "history.hpp"

namespace history {

Entry::Entry(const RewardInternal& p)
    : hash(p.hash())
    , data(RewardData {
          p.toAccountId,
          p.amount })
{
}

Entry::Entry(const VerifiedWartTransfer& p)
    : hash(p.hash)
    , data(WartTransferData {
          p.ti.pinNonce,
          p.ti.compactFee,
          p.ti.origin.id,
          p.ti.to.id,
          p.ti.amount,
      })
{
}

Entry::Entry(const VerifiedTokenTransfer& p, TokenId tokenId)
    : hash(p.hash)
    , data(TokenTransferData {
          p.ti.pinNonce,
          p.ti.compactFee,
          tokenId,
          p.txid.accountId,
          p.ti.to.id,
          p.ti.amount,
      })
{
}
Entry::Entry(const VerifiedOrder& p)
    : hash(p.hash)
    , data(OrderData {
          p.order.pinNonce,
          p.order.compactFee,
          p.order.amount.tokenId,
          p.order.buy,
          p.txid.accountId,
          p.order.limit,
          p.order.amount.funds,
      })
{
}

Entry::Entry(const VerifiedCancelation& p)
    : hash(p.hash)
    , data(CancelationData {
          p.cancelation.pinNonce,
          p.cancelation.compactFee,
          p.cancelation.cancelTxid })
{
}

Entry::Entry(const VerifiedTokenCreation& p, TokenId tokenId)
    : hash(p.hash)
    // = ICombine<3, PinNonceEl, CompactFeeEl, TokenIdEl, OwnerIdEl, TokenSupplyEl, TokenNameEl>;
    , data(TokenCreationData {
          p.tci.pinNonce,
          p.tci.compactFee,
          tokenId,
          p.tci.origin.id,
          p.tci.supply,
          p.tci.name,
      })
{
}

Entry::Entry(const VerifiedLiquidityDeposit& p, Funds_uint64 receivedShares, TokenId tokenId)
    : hash(p.hash)
    , data(LiquidityDeposit {
          p.liquidityAdd.pinNonce,
          p.liquidityAdd.compactFee,
          p.liquidityAdd.basequote.base(),
          p.liquidityAdd.basequote.quote(),
          receivedShares,
          tokenId })
{
}

Entry::Entry(const VerifiedLiquidityWithdrawal& w, Funds_uint64 receivedBase, Wart receivedQuote, TokenId tokenId)
    : hash(w.hash)
    , data(LiquidityWithdraw {
          w.liquidityAdd.pinNonce,
          w.liquidityAdd.compactFee,
          receivedBase,
          receivedQuote,
          w.liquidityAdd.poolShares,
          tokenId })
{
}

Entry::Entry(Hash hash, MatchData m)
    : hash(std::move(hash))
    , data(std::move(m))
{
}

// // do metaprogramming dance
// //
// template <typename V, uint8_t prevcode>
// V check(uint8_t, Reader&)
// {
//     throw std::runtime_error("Cannot parse history entry, unknown variant type");
// }
//
// template <typename V, uint8_t prevIndicator, typename T, typename... S>
// V check(uint8_t indicator, Reader& r)
// {
//     // variant indicators must be all different and in order
//     static_assert(prevIndicator < T::indicator);
//     if (T::indicator == indicator)
//         return T(r);
//     return check<V, T::indicator, S...>(indicator, r);
// }
//
// template <typename Variant, typename T, typename... S>
// Variant check_first(uint8_t indicator, Reader& r)
// {
//     if (T::indicator == indicator)
//         return T(r);
//     return check<Variant, T::indicator, S...>(indicator, r);
// }
//
// template <typename... Types>
// auto parse_recursive(Reader& r)
// {
//     using Variant = wrt::variant<Types...>;
//     auto indicator { r.uint8() };
//     return check_first<Variant, Types...>(indicator, r);
// }
//
// template <typename T>
// class VariantParser {
// };
//
// template <typename... Types>
// struct VariantParser<wrt::variant<Types...>> {
//     static Data parse(std::vector<uint8_t>& v)
//     {
//         Reader r(v);
//         auto res { parse_recursive<Types...>(r) };
//         if (!r.eof())
//             throw Error(EMSGINTEGRITY);
//         return res;
//     }
// };
//
// Data Data::parse_throw(std::vector<uint8_t> v)
// {
//     Reader r(v);
//     using AA = HistoryVariant<WartTransferData>;
//     AA a(v);
//
//     return VariantParser<data_t>::parse(v);
// }
}
