#pragma once
#include "block/body/account_id.hpp"
#include "block/chain/height.hpp"
#include "defi/token/token.hpp"
struct TokenInfo : public TokenIdHashName {
    NonzeroHeight height;
    AccountId ownerAccountId;
    Funds_uint64 totalSupply;
    TokenId group_id;
    std::optional<TokenId> parent_id;
};
