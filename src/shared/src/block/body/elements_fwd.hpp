#pragma once
#include "general/base_elements_fwd.hpp"
namespace block {
namespace body {

template <typename... Ts>
struct Combined;
template <typename... Ts>
struct SignedCombined;

using Reward = Combined<ToIdElement, WartElement>;
using TokenCreation = SignedCombined<TokenSupplyElement, TokenPrecisionElement, TokenNameElement>;
using Transfer = SignedCombined<ToIdElement, AmountElement>;
using Order = SignedCombined<BuyElement, AmountElement, LimitPriceElement>;
using Cancelation = SignedCombined<CancelPinNonceElement>;
using LiquidityAdd = SignedCombined<WartElement, AmountElement>;
using LiquidityRemove = SignedCombined<AmountElement>;
struct TokenTransfer;
struct WartTransfer;
}
}
