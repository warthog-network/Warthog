#pragma once
#include "block/body/account_id.hpp"
#include "block/chain/height.hpp"
#include "defi/token/token.hpp"
// struct AssetBasic {
//     AssetId id;
//     AssetHash hash;
//     AssetName name;
//     AssetPrecision precision;
// };
struct AssetDetailData {
    NonzeroHeight height;
    AccountId ownerAccountId;
    Funds_uint64 totalSupply;
    TokenId group_id;
    wrt::optional<TokenId> parent_id;
};
struct AssetDetail: public AssetBasic, public AssetDetailData {
};
