#pragma once

#include "api/types/forward_declarations.hpp"
#include "block/block_fwd.hpp"
#include "block/chain/height_header_work.hpp"
#include "block/chain/range.hpp"
#include "chainserver/state/helpers/cache.hpp"
#include "communication/messages.hpp"
#include "communication/mining_task.hpp"
#include "communication/stage_operation/result.hpp"
#include "defi/token/info.hpp"
#include "general/result.hpp"
#include "helpers/consensus.hpp"
#include "helpers/past_chains.hpp"
#include <chrono>

namespace chainserver {
struct MiningCache {
    using value_t = block::body::Body;
    struct CacheValidity {
        int db { 0 };
        int mempool { 0 };
        uint32_t timestamp;
        bool operator==(const CacheValidity&) const = default;
        CacheValidity(int db, int mempool, uint32_t timestamp)
            : db(db)
            , mempool(mempool)
            , timestamp(timestamp)
        {
        }
    };
    MiningCache(CacheValidity cacheValidity)
        : cacheValidity(cacheValidity)
    {
    }

    struct Item {
        Address address;
        bool disableTxs;
        value_t b;
    };

    CacheValidity cacheValidity;
    uint32_t timestamp;
    void update_validity(CacheValidity);
    [[nodiscard]] const value_t* lookup(const Address&, bool disableTxs) const;
    const value_t& insert(const Address& a, bool disableTxs, value_t);
    std::vector<Item> cache;
};

class State {
    friend class ApplyStageTransaction;
    friend class SetSignedPinTransaction;
    using StateUpdate = state_update::StateUpdate;
    using StateUpdateWithAPIBlocks = state_update::StateUpdateWithAPIBlocks;
    using StageUpdate = state_update::StageUpdate;
    using TxVec = std::vector<WartTransferMessage>;

public:
    // constructor/destructor
    State(ChainDB& b, BatchRegistry&, std::optional<SnapshotSigner> snapshotSigner);

    // concurrent methods
    Batch get_headers_concurrent(BatchSelector selector) const;
    std::optional<HeaderView> get_header_concurrent(Descriptor descriptor, Height height) const;
    ConsensusSlave get_chainstate_concurrent();

    // normal methods
    void garbage_collect();
    auto mining_task(const Address& a) -> Result<ChainMiningTask>;
    auto mining_task(const Address& a, bool disableTxs) -> Result<ChainMiningTask>;

    auto append_gentx(const WartTransferCreate&) -> std::pair<mempool::Updates, TxHash>;
    auto chainlength() const -> Height { return chainstate.headers().length(); }

    // mempool
    [[nodiscard]] auto insert_txs(const TxVec&) -> std::pair<std::vector<Error>, mempool::Updates>;

    // stage methods
    auto set_stage(Headerchain&& hc) -> stage_operation::StageSetStatus;
    struct StageActionResult {
        stage_operation::StageAddStatus status;
        std::optional<RogueHeaderData> rogueHeaderData;
        std::optional<state_update::StateUpdateWithAPIBlocks> update;
    };
    auto add_stage(const std::vector<Block>& blocks, const Headerchain&) -> StageActionResult;

    // synced state notification
    void set_sync_state(bool synced)
    {
        if (synced) {
            signAfter = std::min(signAfter,
                std::chrono::steady_clock::now() + std::chrono::seconds(5));
        } else {
            signAfter = tp::max();
        }
    }

    // general getters
    auto get_header(Height h) const -> std::optional<std::pair<NonzeroHeight, Header>>;
    auto get_headers() const { return chainstate.headers(); }
    auto get_hash(Height h) const -> std::optional<Hash>;
    auto get_body_data(DescriptedBlockRange) const -> std::vector<BodyData>;
    auto get_mempool_tx(TransactionId) const -> std::optional<WartTransferMessage>;

    // api getters
    auto api_get_address(AddressView) const -> api::WartBalance;
    auto api_get_address(AccountId) const -> api::WartBalance;
    auto api_get_token_balance(const api::AccountIdOrAddress&, const api::TokenIdOrHash&) const -> api::WartBalance;
    auto api_get_head() const -> api::ChainHead;
    auto api_get_history(Address a, int64_t beforeId = 0x7fffffffffffffff) const -> std::optional<api::AccountHistory>;
    auto api_get_richlist(size_t N) const -> api::Richlist;
    auto api_get_mempool(size_t) const -> api::MempoolEntries;
    auto api_get_tx(const TxHash& hash) const -> std::optional<api::Transaction>;
    auto api_get_latest_txs(size_t N = 100) const -> api::TransactionsByBlocks;
    auto api_get_latest_blocks(size_t N = 100) const -> api::TransactionsByBlocks;
    auto api_get_miner(NonzeroHeight h) const -> std::optional<api::AddressWithId>;
    auto api_get_latest_miners(uint32_t N = 1000) const -> std::vector<api::AddressWithId>;
    auto api_get_miners(HeightRange) const -> std::vector<api::AddressWithId>;
    auto api_get_transaction_range(HistoryId lower, HistoryId upper) const -> api::TransactionsByBlocks;
    auto api_get_header(api::HeightOrHash& h) const -> std::optional<std::pair<NonzeroHeight, Header>>;
    auto api_get_block(const api::HeightOrHash& h) const -> std::optional<api::Block>;
    auto api_tx_cache() const -> const TransactionIds;
    size_t api_db_size() const;

private:
    std::optional<AssetInfo> db_lookup_token(const api::TokenIdOrHash&) const;
    // delegated getters
    auto api_get_block(Height h) const -> std::optional<api::Block>;
    std::optional<NonzeroHeight> consensus_height(const Hash&) const;
    NonzeroHeight next_height() const { return chainlength().add1(); }

    // transactions

    struct ApplyStageResult {
        stage_operation::StageAddStatus status;
        std::optional<state_update::StateUpdateWithAPIBlocks> update;
    };
    [[nodiscard]] auto apply_stage(ChainDBTransaction&& t) -> ApplyStageResult;

public:
    [[nodiscard]] auto apply_signed_snapshot(SignedSnapshot&& sp) -> std::optional<StateUpdateWithAPIBlocks>;
    //  stageUpdate;
    [[nodiscard]] auto append_mined_block(const Block&) -> StateUpdateWithAPIBlocks;

private:
    api::Transaction api_dispatch_mempool(const TxHash&, TransactionMessage&&) const;
    api::Transaction api_dispatch_history(const TxHash&, history::HistoryVariant&&, NonzeroHeight) const;

    // transaction helpers
    [[nodiscard]] chainserver::RollbackResult rollback(const Height newlength) const;

    // finalize helpers
    [[nodiscard]] auto commit_fork(RollbackResult&& rr, AppendBlocksResult&&) -> StateUpdate;
    [[nodiscard]] auto commit_append(AppendBlocksResult&& abr) -> StateUpdate;
    std::optional<SignedSnapshot> try_sign_chainstate();
    MiningCache::CacheValidity mining_cache_validity();

private:
    using tp = std::chrono::steady_clock::time_point;
    ChainDB& db;
    mutable DBCache dbcache;
    BatchRegistry& batchRegistry;

    std::optional<SnapshotSigner> snapshotSigner;
    std::optional<SignedSnapshot> signedSnapshot;

    int dbCacheValidity { 0 };
    tp signAfter { tp::max() };
    bool signingEnabled { true };

    mutable std::mutex chainstateMutex; // protects pastChains and chainstate
    BlockCache blockCache;
    chainserver::Chainstate chainstate;

    ExtendableHeaderchain stage;
    std::chrono::steady_clock::time_point nextGarbageCollect;

    MiningCache _miningCache;
};
}
