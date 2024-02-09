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
    auto mining_task(const Address& a, bool log) -> MiningTask;

    auto append_gentx(const PaymentCreateMessage&) -> tl::expected<mempool::Log, Error>;
    auto chainlength() const -> Height { return chainstate.headers().length(); }

    // mempool
    [[nodiscard]] auto insert_txs(const TxVec&) -> std::pair<std::vector<int32_t>, mempool::Log>;

    // stage methods
    auto set_stage(Headerchain&& hc) -> stage_operation::StageSetResult;
    auto add_stage(const std::vector<Block>& blocks, const Headerchain&) -> std::pair<stage_operation::StageAddResult, std::optional<StateUpdate>>;

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
    auto api_get_address(AddressView) -> API::Balance;
    auto api_get_head() const -> API::Head;
    auto api_get_history(Address a, uint64_t beforeId) -> std::optional<API::AccountHistory>;
    auto api_get_richlist(size_t N) -> API::Richlist;
    auto api_get_mempool(size_t) -> API::MempoolEntries;
    auto api_get_tx(HashView hash) const -> std::optional<API::Transaction>;
    auto api_get_latest_txs(size_t N = 100) const -> API::TransactionsByBlocks;
    auto api_get_header(API::HeightOrHash& h) const -> std::optional<std::pair<NonzeroHeight, Header>>;
    auto api_get_block(const API::HeightOrHash& h) const -> std::optional<API::Block>;
    auto api_tx_cache() const -> const TransactionIds;

private:
    // delegated getters
    auto api_get_block(Height h) const -> std::optional<API::Block>;
    std::optional<NonzeroHeight> consensus_height(const Hash&) const;
    NonzeroHeight next_height() const { return (chainlength() + 1).nonzero_assert(); }

    // transactions
    [[nodiscard]] auto apply_stage(ChainDBTransaction&& t) -> std::tuple<ChainError, std::optional<StateUpdate>, std::vector<API::Block>>;

public:
    [[nodiscard]] auto apply_signed_snapshot(SignedSnapshot&& sp) -> std::optional<StateUpdate>;
    //  stageUpdate;
    [[nodiscard]] auto append_mined_block(const Block&) -> StateUpdate;

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
