#pragma once
#include "general/base_elements_fwd.hpp"
#include "general/structured_reader_fwd.hpp"
namespace block {
namespace body {

template <typename... Ts>
struct Combined;
template <typename... Ts>
struct SignedCombined;
template <StaticString tag, typename... Ts>
using TaggedSignedCombined = Tag< tag,SignedCombined<Ts...>,true>;

using Reward = Combined<ToAccIdEl, WartEl>;
using WartTransfer = TaggedSignedCombined<"wartTransfer", ToAccIdEl, WartEl>;
using AssetTransfer = TaggedSignedCombined<"assetTransfer", ToAccIdEl, AmountEl>;
using ShareTransfer = TaggedSignedCombined<"shareTransfer", ToAccIdEl, SharesEl>;
using AssetCreation = TaggedSignedCombined<"assetCreation", AssetSupplyEl, AssetNameEl>;
using Order = TaggedSignedCombined<"order", BuyEl, AmountEl, LimitPriceEl>;
struct CancelationBase;
using Cancelation = Tag<"cancelation", CancelationBase,true>;
using LiquidityDeposit = TaggedSignedCombined<"liquidityDeposit", QuoteEl, BaseEl>;
using LiquidityWithdrawal = TaggedSignedCombined<"liquidityWithdrawal", AmountEl>;
}
}
