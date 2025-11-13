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
using TaggedSignedCombined = Tag<tag, SignedCombined<Ts...>>;

using Reward = Combined<ToAccIdEl, WartEl>;
using WartTransfer = TaggedSignedCombined<"wartTransfer", ToAccIdEl, WartEl>;
using AssetTransfer = TaggedSignedCombined<"assetTransfer", ToAccIdEl, NonzeroAmountEl>;
using ShareTransfer = TaggedSignedCombined<"shareTransfer", ToAccIdEl, NonzeroSharesEl>;
using AssetCreation = TaggedSignedCombined<"assetCreation", AssetSupplyEl, AssetNameEl>;
using Order = TaggedSignedCombined<"order", BuyEl, NonzeroAmountEl, LimitPriceEl>;
struct CancelationBase;
using Cancelation = Tag<"cancelation", CancelationBase>;
using LiquidityDeposit = TaggedSignedCombined<"liquidityDeposit", BaseEl, QuoteEl>;
using LiquidityWithdrawal = TaggedSignedCombined<"liquidityWithdrawal", NonzeroAmountEl>;
}
}
