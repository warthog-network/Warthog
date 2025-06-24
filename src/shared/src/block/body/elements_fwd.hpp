#pragma once
#include "general/base_elements_fwd.hpp"
namespace block {
namespace body {

template <typename... Ts>
struct Combined;
template <typename... Ts>
struct SignedCombined;

using Reward = Combined<ToAccIdEl, WartEl>;
using AssetCreation = SignedCombined<AssetSupplyEl, AssetPrecisionEl, AssetNameEl>;
using WartTransfer = SignedCombined<ToAccIdEl, WartEl>;
using TokenTransfer = SignedCombined<ToAccIdEl, AmountEl>;
using ShareTransfer = SignedCombined<ToAccIdEl, AmountEl>;
using Order = SignedCombined<BuyEl, AmountEl, LimitPriceEl>;
using Cancelation = SignedCombined<CancelPinNonceEl>;
using LiquidityDeposit = SignedCombined<QuoteWartEl, BaseAmountEl>;
using LiquidityWithdraw = SignedCombined<AmountEl>;
}
}
