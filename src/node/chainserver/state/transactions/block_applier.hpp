#pragma once
#include "../../transaction_ids.hpp"
#include "api/types/forward_declarations.hpp"
#include "block/block_fwd.hpp"
#include "chainserver/db/types_fwd.hpp"
#include "crypto/hash.hpp"

class Headerchain;
class BlockId;

namespace chainserver {
class Preparation;
struct BlockApplier {
    BlockApplier(ChainDB& db, const Headerchain& hc, const std::set<TransactionId, ByPinHeight>& baseTxIds, bool fromStage)
        : preparer { db, hc, baseTxIds, {} }
        , db(db)
        , fromStage(fromStage)
    {
    }
    TransactionIds&& move_new_txids() { return std::move(preparer.newTxIds); };
    auto&& move_balance_updates() { return std::move(WART_BalanceUpdates); };
    [[nodiscard]] api::CompleteBlock apply_block(const Block& b, const BlockHash& hash, BlockId blockId);

private: // private methods
    friend class Preparation;
    friend class PreparationGenerator;
    struct Preparer {
        const ChainDB& db; // preparer cannot modify db!
        const Headerchain& hc;
        const std::set<TransactionId, ByPinHeight>& baseTxIds;
        TransactionIds newTxIds;
        Preparation prepare(const Block&, const Hash& h) const;
    };

private: // private data
    Preparer preparer;
    std::map<AccountId, Wart> WART_BalanceUpdates;
    ChainDB& db;
    bool fromStage;
};
}
