#pragma once
#include "../../transaction_ids.hpp"
#include "api/types/forward_declarations.hpp"
#include "defi/token/account_token.hpp"
class ChainDB;
class Headerchain;
class BodyView;
class BlockId;
class ParsedBlock;

namespace chainserver {
struct Preparation;
struct BlockApplier {
    BlockApplier(ChainDB& db, const Headerchain& hc, const std::set<TransactionId, ByPinHeight>& baseTxIds, bool fromStage)
        : preparer { db, hc, baseTxIds, {} }
        , db(db)
        , fromStage(fromStage)
    {
    }
    TransactionIds&& move_new_txids() { return std::move(preparer.newTxIds); };
    auto&& move_balance_updates() { return std::move(balanceUpdates); };
    [[nodiscard]] api::Block apply_block(const ParsedBlock& bv, BlockId blockId);

private: // private methods
    friend struct Preparation;
    struct Preparer {
        const ChainDB& db; // preparer cannot modify db!
        const Headerchain& hc;
        const std::set<TransactionId, ByPinHeight>& baseTxIds;
        TransactionIds newTxIds;
        Preparation prepare(const ParsedBlock&) const;
    };

private: // private data
    Preparer preparer;
    std::map<AccountToken,Funds_uint64> balanceUpdates;
    ChainDB& db;
    bool fromStage;
};
}
