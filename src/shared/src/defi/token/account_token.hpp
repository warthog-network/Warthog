#pragma once
#include "block/body/account_id.hpp"
#include "defi/token/id.hpp"

struct AccountToken {
public:
    AccountToken(AccountId accountId, TokenId tokenId)
        : accountId(accountId)
        , tokenId(tokenId)
    {
    }
    auto account_id() const { return accountId; }
    auto& token_id() const { return tokenId; }
    auto& token_id() { return tokenId; }
    auto operator<=>(const AccountToken&) const = default;

private:
    AccountId accountId;
    TokenId tokenId;
};

struct CreatorToken : protected AccountToken {
    CreatorToken(AccountId creatorId, TokenId tokenId)
        : AccountToken(creatorId, tokenId)
    {
    }
    auto creator_id() const { return account_id(); }
    using AccountToken::token_id;
    auto operator<=>(const CreatorToken&) const = default;
};
