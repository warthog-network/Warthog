#pragma once
#include "../../account_cache.hpp"
#include "../../transaction_ids.hpp"
#include "../update/update.hpp"
#include "block/body/account_id.hpp"
#include "block/chain/consensus_headers.hpp"
#include "block/chain/offsts.hpp"
#include "mempool/mempool.hpp"
#include "db/chain/deletion_key.hpp"
#include <cstdint>

class ChainDB;
namespace chainserver {
struct RollbackResult {
    Height shrinkLength;
    std::vector<TransferTxExchangeMessage> toMempool;
    TransactionIds chainTxIds;
    DeletionKey deletionKey;
};
struct AppendBlocksResult {
    std::vector<uint64_t> newHistoryOffsets;
    std::vector<AccountId> newAccountOffsets;
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
        std::optional<SignedSnapshot>& signedSnapshot;
        HeaderVerifier::PreparedAppend prepared;
        TransactionIds&& newTxIds;
        uint64_t newHistoryOffset;
        AccountId newAccountOffset;
    };
    Chainstate(const ChainDB& db, BatchRegistry& br);

    // modification functions
    mempool::Log pop_mempool_log();

    using Update = state_update::StateUpdate;

    void fork(ForkData&&);
    [[nodiscard]] auto rollback(const RollbackResult&) -> HeaderchainRollback;
    [[nodiscard]] auto append(AppendMulti) -> HeaderchainAppend;
    [[nodiscard]] auto append(AppendSingle) -> HeaderchainAppend;

    [[nodiscard]] int32_t insert_tx(const TransferTxExchangeMessage& m);
    [[nodiscard]] int32_t insert_tx(const PaymentCreateMessage& m);

    // const functions
    Worksum work_with_new_block() const{return headerchain.total_work() + headerchain.next_target();};
    const auto& headers() const { return headerchain; }
    auto mining_data() const { return headerchain.mining_data(); };
    HashView final_hash() const { return headerchain.final_hash(); }
    auto prepare_append(const std::optional<SignedSnapshot>& sp, HeaderView hv) const { return headerchain.prepare_append(sp, hv); }
    Height length() const { return headerchain.length(); }
    Descriptor descriptor() const { return dsc; }
    const auto& txids() const { return chainTxIds; }
    const auto& mempool() const { return _mempool; }
    inline size_t historyOffset(NonzeroHeight height) const
    {
        return historyOffsets.at(height);
    };
    NonzeroHeight history_height(uint64_t historyIndex) const
    {
        return historyOffsets.height(historyIndex);
    }
    auto account_height(AccountId id) const
    {
        return accountOffsets.height(id);
    }

protected:
    void prune_txids();
    Chainstate(std::tuple<std::vector<Batch>, HistoryHeights, AccountHeights> init,
        const ChainDB& db, BatchRegistry& br);

private:
    const ChainDB& db;
    uint32_t dsc = 0;
    void assert_equal_length();
    ExtendableHeaderchain headerchain;
    HistoryHeights historyOffsets;
    AccountHeights accountOffsets;
    TransactionIds chainTxIds; // replay protection
    mempool::Mempool _mempool;
};

}
