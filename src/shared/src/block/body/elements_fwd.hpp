#pragma once
namespace block {
namespace body {
struct DestinationIdElement;
struct WartElement;
struct AmountElement;
struct OriginAccountIdElement;
struct CancelPinNonceElement;
struct PinNonceElement;
struct CompactFeeElement;
struct TokenSupplyElement;
struct TokenPrecisionElement;
struct TokenNameElement;
struct SignatureElement;
struct LimitPriceElement;
struct BuyElement;

template <typename... Ts>
struct Combined;
template <typename... Ts>
struct SignedCombined;

using Reward = Combined<DestinationIdElement, WartElement>;
using TokenCreation = SignedCombined<TokenSupplyElement, TokenPrecisionElement, TokenNameElement>;
using Transfer = SignedCombined<DestinationIdElement, AmountElement>;
using Order = SignedCombined<BuyElement, AmountElement, LimitPriceElement>;
using Cancelation = SignedCombined<CancelPinNonceElement>;
using LiquidityAdd = SignedCombined<WartElement, AmountElement>;
using LiquidityRemove = SignedCombined<AmountElement>;
struct TokenTransfer;
struct WartTransfer;
}
}
