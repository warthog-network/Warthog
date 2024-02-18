#pragma once
#include "crypto/address.hpp"
#include "../../transaction_ids.hpp"
#include "api/types/forward_declarations.hpp"
class ChainDB;
class Headerchain;
class BodyView;
class BlockId;
class HeaderView;

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
    [[nodiscard]] API::Block apply_block(const BodyView& bv, HeaderView, NonzeroHeight height, BlockId blockId);

private: // private methods
    struct Preparer {
        const ChainDB& db; // preparer cannot modify db!
        const Headerchain& hc;
        const std::set<TransactionId, ByPinHeight>& baseTxIds;
        TransactionIds newTxIds;
        Preparation prepare(const BodyView& bv, const NonzeroHeight height) const;
    };

private: // private data
    Preparer preparer;
    std::map<AccountId,Funds> balanceUpdates;
    ChainDB& db;
    bool fromStage;
};
}
