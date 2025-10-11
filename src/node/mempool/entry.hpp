#pragma once

#include "block/body/messages.hpp"
#include "defi/token/account_token.hpp"
namespace mempool {
struct Entry : public TransactionMessage {
    TxHash txhash;
    TxHeight txHeight; // when was the account first registered
    TokenId altTokenId;
    AccountToken account_token() const { return { from_id(), altTokenId }; }
    Entry(TransactionMessage m, const TxHash& txHash, TxHeight txHeight, TokenId altToken)
        : TransactionMessage(std::move(m))
        , txhash(txHash)
        , txHeight(txHeight)
        , altTokenId(std::move(altToken))
    {
    }
};
}
