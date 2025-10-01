#pragma once
#include "general/base_elements_fwd.hpp"
namespace block {
namespace body {

template <typename... Ts>
struct Combined;
template <typename... Ts>
struct SignedCombined;

using Reward = Combined<ToAccIdEl, WartEl>;
using WartTransfer = SignedCombined<ToAccIdEl, WartEl>;
using TokenTransfer = SignedCombined<ToAccIdEl, AmountEl>;
using ShareTransfer = SignedCombined<ToAccIdEl, SharesEl>;
using AssetCreation = SignedCombined<AssetSupplyEl, AssetNameEl>;
using Order = SignedCombined<BuyEl, AmountEl, LimitPriceEl>;
struct Cancelation;
using LiquidityDeposit = SignedCombined<QuoteWartEl, BaseAmountEl>;
using LiquidityWithdraw = SignedCombined<AmountEl>;
}
}
