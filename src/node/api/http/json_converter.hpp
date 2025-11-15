#pragma once

#include "general/base_elements.hpp"
#include "nlohmann/json_fwd.hpp"

struct JSONConverter {
    const nlohmann::json& json;
    JSONConverter(const nlohmann::json& json)
        : json(std::move(json))
    {
    }
    Address to_addr() const;
    operator ToAddrEl() const;
    AssetHash asset_hash() const;
    operator AssetHashEl() const;
    auto liquidity_flag() const;
    operator LiquidityFlagEl() const;
    NonceId nonce_id() const;
    operator NonceIdEl() const;
    NonceId cancel_nonce_id() const;
    operator CancelNonceEl() const;
    CompactUInt fee() const;
    operator CompactFeeEl() const;
    PinHeight pin_height() const;
    operator PinHeightEl() const;
    Wart wart() const;
    operator WartEl() const;
    operator NonzeroWartEl() const;
    Funds_uint64 amount() const;
    operator AmountEl() const;
    bool buy() const;
    operator BuyEl() const;
    Price_uint64 limit() const;
    operator LimitPriceEl() const;
    RecoverableSignature signature() const;
    operator SignatureEl() const;
    PinHeight cancel_height() const;
    operator CancelHeightEl() const;
    AssetName asset_name() const;
    operator AssetNameEl() const;
    Funds_uint64 asset_units() const;
    AssetPrecision asset_precision() const;
    FundsDecimal asset_supply() const;
    operator AssetSupplyEl() const;
};
