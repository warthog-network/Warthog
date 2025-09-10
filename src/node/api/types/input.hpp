#pragma once

#include "block/body/account_id.hpp"
#include "crypto/address.hpp"
#include "crypto/hash.hpp"
#include "defi/token/id.hpp"
#include "tools/alternative.hpp"
class Reader;

namespace api {
struct AssetIdOrHash : public wrt::alternative<AssetId, AssetHash> {
    using alternative::alternative;
};
struct TokenSpec {
    AssetHash assetHash;
    bool liquidityDerivative;
    TokenSpec(AssetHash ah, bool liquidity)
    :assetHash(std::move(ah)), liquidityDerivative(liquidity)
    {
    }

    static TokenSpec parse_throw(std::string_view s){
        if (auto o{parse(s)})
            return *o;
        throw Error(EINV_TOKEN);
    }

    static std::optional<TokenSpec> parse(std::string_view s)
    {
        auto pos { s.find(":") };
        if (pos == s.npos)
            return {};
        auto indicatorStr { s.substr(0, pos) };
        bool liquidity;
        if (indicatorStr == "liquidity") {
            liquidity = true;
        } else if (indicatorStr == "asset") {
            liquidity = false;
        } else {
            return {};
        }
        auto hashStr { s.substr(pos + 1) };
        auto ah { AssetHash::parse_string(hashStr) };
        if (!ah)
            return {};
        return TokenSpec { *ah, liquidity };
    }
};
struct TokenIdOrSpec : public wrt::alternative<TokenId, TokenSpec> {
    using alternative::alternative;
};
struct AccountIdOrAddress : public wrt::alternative<AccountId, Address> {
    using alternative::alternative;
};
}
