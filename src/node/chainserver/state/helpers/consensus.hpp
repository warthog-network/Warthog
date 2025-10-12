#pragma once
#include "../../transaction_ids.hpp"
#include "../update/update.hpp"
#include "block/body/account_id.hpp"
#include "block/chain/consensus_headers.hpp"
#include "block/chain/history/index.hpp"
#include "block/chain/offsts.hpp"
#include "cache_fwd.hpp"
#include "chainserver/db/deletion_key.hpp"
#include "chainserver/db/state_ids.hpp"
#include "chainserver/db/types_fwd.hpp"
#include "chainserver/types.hpp"
#include "mempool/mempool.hpp"
#include <cstdint>

namespace chainserver {
struct RollbackResult {
    ShrinkInfo shrink;
    std::vector<TransactionMessage> toMempool;
    free_balance_udpates_t freeBalanceUpdates;
    TransactionIds chainTxIds;
    DeletionKey deletionKey;
};
struct AppendBlocksResult {
    free_balance_udpates_t freeBalanceUpdates;
    std::vector<HistoryId> newHistoryOffsets;
    std::vector<StateId64> stateOffsets;
    TransactionIds newTxIds;
};

struct Chainstate {
    struct RollbackData {
        RollbackResult&& rollbackResult;
    };
    struct ForkData {
        ExtendableHeaderchain stage;
        RollbackResult&& rollbackResult;
        AppendBlocksResult&& appendResult;
    };
    struct AppendMulti {
        ExtendableHeaderchain patchedChain;
        AppendBlocksResult&& appendResult;
    };
    struct AppendSingle {
        free_balance_udpates_t freeBalanceUpdates;
        std::optional<SignedSnapshot>& signedSnapshot;
        HeaderVerifier::PreparedAppend prepared;
        TransactionIds&& newTxIds;
        HistoryId newHistoryOffset;
        StateId64 newStateOffset;
    };
    Chainstate(const ChainDB& db, BatchRegistry& br);

    // modification functions
    mempool::Updates pop_mempool_updates();

    using Update = state_update::StateUpdate;

    void fork(ForkData&&);
    [[nodiscard]] auto rollback(const RollbackResult&) -> HeaderchainRollback;
    [[nodiscard]] auto append(AppendMulti) -> HeaderchainAppend;
    [[nodiscard]] auto append(AppendSingle) -> HeaderchainAppend;

    auto insert_txs(const std::vector<TransactionMessage>& txs) -> std::vector<Error>;

    TxHash insert_tx(const TransactionMessage& m, DBCache& ac);
    [[nodiscard]] TxHash create_tx(const WartTransferCreate& m);

    // const functions
    Worksum work_with_new_block() const { return headerchain.total_work() + headerchain.next_target(); };
    const auto& headers() const { return headerchain; }
    auto mining_data() const { return headerchain.mining_data(); };
    const Hash& final_hash() const { return headerchain.final_hash(); }
    auto prepare_append(const std::optional<SignedSnapshot>& sp, HeaderView hv) const { return headerchain.prepare_append(sp, hv); }
    Height length() const { return headerchain.length(); }
    Descriptor descriptor() const { return dsc; }
    const auto& txids() const { return chainTxIds; }
    const auto& mempool() const { return _mempool; }
    [[nodiscard]] inline auto history_offset(NonzeroHeight height) const
    {
        if (height.value() == 1)
            return HistoryId::smallest();
        return historyOffsets.at(height);
    };
    NonzeroHeight history_height(HistoryId historyIndex) const
    {
        return historyOffsets.height(historyIndex);
    }
    StateHeight account_height(AccountId id) const
    {
        return stateOffsets.height(state_id(id));
    }

protected:
    TxHash insert_tx_internal(const TransactionMessage&, TxHeight, TxHash, DBCache&, const Address fromAddr);
    void prune_txids();
    Chainstate(std::tuple<std::vector<Batch>, HistoryHeights, State64Heights> init,
        const ChainDB& db, BatchRegistry& br);

private:
    const ChainDB& db;
    uint32_t dsc = 0;
    void assert_equal_length();
    ExtendableHeaderchain headerchain;
    HistoryHeights historyOffsets;
    State64Heights stateOffsets;
    TransactionIds chainTxIds; // replay protection
    mempool::Mempool _mempool;
};

}
