#pragma once
#include "api/types/forward_declarations.hpp"
#include "block/chain/range.hpp"
#include "communication/messages.hpp"
#include "communication/mining_task.hpp"
#include "communication/stage_operation/result.hpp"
#include "helpers/consensus.hpp"
#include "helpers/past_chains.hpp"
#include <chrono>

class ChainDB;
struct Block;

class ChainDBTransaction;
namespace chainserver {
class State {
    friend class ApplyStageTransaction;
    friend class SetSignedPinTransaction;
    using StateUpdate = state_update::StateUpdate;
    using StateUpdateWithAPIBlocks = state_update::StateUpdateWithAPIBlocks;
    using StageUpdate = state_update::StageUpdate;
    using TxVec = std::vector<TransferTxExchangeMessage>;

public:
    // constructor/destructor
    State(ChainDB& b, BatchRegistry&, std::optional<SnapshotSigner> snapshotSigner);

    // concurrent methods
    Batch get_headers_concurrent(BatchSelector selector);
    std::optional<HeaderView> get_header_concurrent(Descriptor descriptor, Height height);
    ConsensusSlave get_chainstate_concurrent();

    // normal methods
    void garbage_collect();
    auto mining_task(const Address& a) -> tl::expected<ChainMiningTask,Error>;

    auto append_gentx(const PaymentCreateMessage&) -> std::pair<mempool::Log, TxHash>;
    auto chainlength() const -> Height { return chainstate.headers().length(); }

    // mempool
    [[nodiscard]] auto insert_txs(const TxVec&) -> std::pair<std::vector<Error>, mempool::Log>;

    // stage methods
    auto set_stage(Headerchain&& hc) -> stage_operation::StageSetStatus;
    struct StageActionResult{
        stage_operation::StageAddStatus status;
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
    auto get_blocks(DescriptedBlockRange) -> std::vector<BodyContainer>;
    auto get_mempool_tx(TransactionId) const -> std::optional<TransferTxExchangeMessage>;

    // api getters
    auto api_get_address(AddressView) -> api::Balance;
    auto api_get_address(AccountId) -> api::Balance;
    auto api_get_head() const -> api::ChainHead;
    auto api_get_history(Address a, int64_t beforeId=0x7fffffffffffffff) -> std::optional<api::AccountHistory>;
    auto api_get_richlist(size_t N) -> api::Richlist;
    auto api_get_mempool(size_t) -> api::MempoolEntries;
    auto api_get_tx(HashView hash) const -> std::optional<api::Transaction>;
    auto api_get_latest_txs(size_t N = 100) const -> api::TransactionsByBlocks;
    auto api_get_latest_blocks(size_t N = 100) const -> api::TransactionsByBlocks;
    auto api_get_miner(NonzeroHeight h) const -> std::optional<api::AddressWithId>;
    auto api_get_latest_miners(uint32_t N=1000) const -> std::vector<api::AddressWithId>;
    auto api_get_miners(HeightRange) const -> std::vector<api::AddressWithId>;
    auto api_get_transaction_range(HistoryId lower, HistoryId upper) const -> api::TransactionsByBlocks;
    auto api_get_header(api::HeightOrHash& h) const -> std::optional<std::pair<NonzeroHeight, Header>>;
    auto api_get_block(const api::HeightOrHash& h) const -> std::optional<api::Block>;
    auto api_tx_cache() const -> const TransactionIds;

private:

    // delegated getters
    auto api_get_block(Height h) const -> std::optional<api::Block>;
    std::optional<NonzeroHeight> consensus_height(const Hash&) const;
    NonzeroHeight next_height() const { return (chainlength() + 1).nonzero_assert(); }

    // transactions

    [[nodiscard]] auto apply_stage(ChainDBTransaction&& t) -> StageActionResult;

public:
    [[nodiscard]] auto apply_signed_snapshot(SignedSnapshot&& sp) -> std::optional<StateUpdateWithAPIBlocks>;
    //  stageUpdate;
    [[nodiscard]] auto append_mined_block(const Block&) -> StateUpdateWithAPIBlocks;

private:
    // transaction helpers
    [[nodiscard]] chainserver::RollbackResult rollback(const Height newlength) const;

    // finalize helpers
    [[nodiscard]] auto commit_fork(RollbackResult&& rr, AppendBlocksResult&&) -> StateUpdate;
    [[nodiscard]] auto commit_append(AppendBlocksResult&& abr) -> StateUpdate;
    std::optional<SignedSnapshot> try_sign_chainstate();

private:
    using tp = std::chrono::steady_clock::time_point;
    ChainDB& db;
    BatchRegistry& batchRegistry;

    std::optional<SnapshotSigner> snapshotSigner;
    std::optional<SignedSnapshot> signedSnapshot;
    tp signAfter { tp::max() };
    bool signingEnabled { true };

    std::mutex chainstateMutex; // protects pastChains and chainstate
    BlockCache blockCache;
    chainserver::Chainstate chainstate;

    ExtendableHeaderchain stage;
    std::chrono::steady_clock::time_point nextGarbageCollect;
};
}
