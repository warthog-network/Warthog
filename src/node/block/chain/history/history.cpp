#include "history.hpp"

namespace history {

Entry::Entry(const RewardInternal& p)
    : hash(p.hash())
    , data(RewardData {
          p.toAccountId,
          p.wart })
{
}

Entry::Entry(const block_apply::WartTransfer::Verified& p)
    : hash(p.hash)
    , data(WartTransferData {
          p.ref.pinNonce,
          p.ref.compactFee,
          p.ref.origin.id,
          p.ref.to_id(),
          p.ref.wart(),
      })
{
}

Entry::Entry(const block_apply::TokenTransfer::Verified& p, TokenId tokenId)
    : hash(p.hash)
    , data(TokenTransferData {
          p.ref.pinNonce,
          p.ref.compactFee,
          tokenId,
          p.txid.accountId,
          p.ref.to_id(),
          p.ref.amount(),
      })
{
}
Entry::Entry(const block_apply::Order::Verified& p)
    : hash(p.hash)
    , data(OrderData {
          p.ref.pinNonce,
          p.ref.compactFee,
          p.ref.asset_id(),
          p.ref.buy(),
          p.txid.accountId,
          p.ref.limit(),
          p.ref.amount(),
      })
{
}

Entry::Entry(const block_apply::Cancelation::Verified& p)
    : hash(p.hash)
    , data(CancelationData {
          p.ref.pinNonce,
          p.ref.compactFee,
          p.ref.cancel_txid() })
{
}

Entry::Entry(const block_apply::AssetCreation::Verified& p, AssetId assetId)
    : hash(p.hash)
    // = ICombine<3, PinNonceEl, CompactFeeEl, AssetIdEl, OwnerIdEl, TokenSupplyEl, TokenNameEl>;
    , data(AssetCreationData {
          p.ref.pinNonce,
          p.ref.compactFee,
          assetId,
          p.ref.origin.id,
          p.ref.supply(),
          p.ref.asset_name(),
      })
{
}

Entry::Entry(const block_apply::LiquidityDeposit::Verified& p, Funds_uint64 receivedShares)
    : hash(p.hash)
    , data(LiquidityDeposit {
          p.ref.pinNonce,
          p.ref.compactFee,
          p.ref.base(),
          p.ref.quote(),
          receivedShares,
          p.ref.asset_id() })
{
}

Entry::Entry(const block_apply::LiquidityWithdrawal::Verified& w, Funds_uint64 receivedBase, Wart receivedQuote)
    : hash(w.hash)
    , data(LiquidityWithdraw {
          w.ref.pinNonce,
          w.ref.compactFee,
          receivedBase,
          receivedQuote,
          w.ref.amount(),
          w.ref.asset_id() })
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
