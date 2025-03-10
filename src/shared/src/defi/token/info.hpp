#pragma once
#include "block/body/account_id.hpp"
#include "block/chain/height.hpp"
#include "crypto/hash.hpp"
#include "defi/token/token.hpp"
struct TokenInfo {
    TokenId id;
    NonzeroHeight height;
    AccountId ownerAccountId;
    Funds_uint64 totalSupply;
    TokenId group_id;
    std::optional<TokenId> parent_id;
    TokenName name;
    TokenHash hash;
};
