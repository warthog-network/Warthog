#include "state.hpp"
#include "api/types/all.hpp"
#include "block/body/generator.hpp"
#include "block/body/parse.hpp"
#include "block/body/rollback.hpp"
#include "block/body/view.hpp"
#include "block/chain/history/history.hpp"
#include "block/header/generator.hpp"
#include "block/header/header_impl.hpp"
#include "communication/create_payment.hpp"
#include "db/chain_db.hpp"
#include "eventloop/types/chainstate.hpp"
#include "general/hex.hpp"
#include "global/globals.hpp"
#include "spdlog/spdlog.h"
#include "transactions/apply_stage.hpp"
#include "transactions/block_applier.hpp"
#include <ranges>
namespace chainserver {

State::State(ChainDB& db, BatchRegistry& br, std::optional<SnapshotSigner> snapshotSigner)
    : db(db)
    , batchRegistry(br)
    , snapshotSigner(std::move(snapshotSigner))
    , signedSnapshot(db.get_signed_snapshot())
    , chainstate(db, br)
    , nextGarbageCollect(std::chrono::steady_clock::now()) {};

std::optional<Header> State::get_header(Height h) const
{
    return chainstate.headers().get_header(h);
};

std::optional<Hash> State::get_hash(Height h) const
{
    return chainstate.headers().get_hash(h);
};

std::optional<API::Block> State::api_get_block(Height zh) const
{
    if (zh == 0 || zh > chainlength())
        return {};
    auto h { zh.nonzero_assert() };
    PinFloor pinFloor { h - 1 };
    size_t lower = chainstate.historyOffset(h);
    size_t upper = (h == chainlength() ? 0
                                       : chainstate.historyOffset(h + 1));
    auto entries = db.lookupHistoryRange(lower, upper);
    auto header = chainstate.headers()[h];
    API::Block b(header, h, chainlength() - h + 1);

    chainserver::AccountCache cache(db);
    for (auto [hash, data] : entries) {
        b.push_history(hash, data, cache, pinFloor);
    }
    return b;
};

auto State::api_tx_cache() const -> const TransactionIds
{
    return chainstate.txids();
}

std::optional<API::Transaction> State::api_get_tx(const HashView txHash) const
{
    if (auto p = chainstate.mempool()[txHash]; p) {
        auto& tx = *p;

        return API::TransferTransaction {
            .txhash = txHash,
            .toAddress = tx.toAddr,
            .confirmations = 0,
            .height = Height(0),
            .amount = tx.amount,
            .fromAddress = tx.from_address(txHash),
            .fee = tx.fee(),
            .nonceId = tx.txid.nonceId,
            .pinHeight = tx.pin_height(),
        };
    }
    auto p = db.lookup_history(txHash);
    if (p) {
        auto& [data, historyIndex] = *p;
        if (data.size() == 0)
            return {};
        auto parsed = history::parse(data);
        if (!parsed)
            throw std::runtime_error("Database corrupted");
        NonzeroHeight h { chainstate.history_height(historyIndex) };
        if (std::holds_alternative<history::TransferData>(*parsed)) {
            auto& d = std::get<history::TransferData>(*parsed);
            return API::TransferTransaction {
                .txhash = txHash,
                .toAddress = db.fetch_account(d.toAccountId).address,
                .confirmations = (chainlength() - h) + 1,
                .height = h,
                .timestamp = chainstate.headers()[h].timestamp(),
                .amount = d.amount,
                .fromAddress = db.fetch_account(d.fromAccountId).address,
                .fee = d.compactFee,
                .nonceId = d.pinNonce.id,
                .pinHeight = d.pinNonce.pin_height((PinFloor(h - 1)))
            };
        } else {
            assert(std::holds_alternative<history::RewardData>(*parsed));
            auto& d = std::get<history::RewardData>(*parsed);
            return API::RewardTransaction {
                .txhash = txHash,
                .toAddress = db.fetch_account(d.toAccountId).address,
                .confirmations = (chainlength() - h) + 1,
                .height = h,
                .timestamp = chainstate.headers()[h].timestamp(),
                .amount = d.miningReward
            };
        }
    }
    return {};
};

void State::garbage_collect()
{
    // garbage collect old unused blocks
    using namespace std::chrono;
    if (auto n = steady_clock::now(); n > nextGarbageCollect) {
        nextGarbageCollect = n + minutes(5);
        auto tr = db.transaction();
        blockCache.garbage_collect(db);
        tr.commit();
    }
}

Batch State::get_headers_concurrent(BatchSelector s)
{
    std::unique_lock<std::mutex> lcons(chainstateMutex);
    if (s.descriptor == chainstate.descriptor()) {
        return chainstate.headers().get_headers(s.startHeight, s.end());
    } else {
        return blockCache.get_batch(s);
    }
}

std::optional<HeaderView> State::get_header_concurrent(Descriptor descriptor, Height height)
{
    std::unique_lock<std::mutex> lcons(chainstateMutex);
    if (descriptor == chainstate.descriptor()) {
        return chainstate.headers().get_header(height);
    } else {
        return blockCache.get_header(descriptor, height);
    }
};

ConsensusSlave State::get_chainstate_concurrent()
{
    std::unique_lock<std::mutex> l(chainstateMutex);
    return { signedSnapshot, chainstate.descriptor(), chainstate.headers() };
};

MiningTask State::mining_task(const Address& a)
{
    auto md = chainstate.mining_data();

    auto payments { chainstate.mempool().get_payments(50) };
    Funds totalfee { 0 };
    for (auto& p : payments)
        totalfee += p.fee();
    std::vector<Payout>
        payouts { { a, md.reward + totalfee } };

    // mempool should have deleted out of window transactions
    auto body { generate_body(db, chainlength() + 1, payouts, payments) };
    BodyView bv(body.view());
    if (!bv.valid())
        spdlog::error("Cannot create mining task, body invalid");

    HeaderGenerator hg(md.prevhash, bv, md.target, md.timestamp);
    return { .block {
        .height = (chainlength() + 1).nonzero_assert(),
        .header = hg.serialize(0),
        .body = std::move(body),
    } };
}

stage_operation::StageSetResult State::set_stage(Headerchain&& hc)
{
    if (signedSnapshot && !signedSnapshot->compatible(hc)) {
        return {};
    }

    auto l { hc.length() };
    auto t = db.transaction();
    NonzeroHeight fh1 { fork_height(chainstate.headers(), hc) };
    NonzeroHeight fh2 { fork_height(stage, hc) };
    std::optional<NonzeroHeight> newProtectBegin;
    if (fh1 >= fh2) {
        newProtectBegin = fh1;
        if (fh1 > fh2)
            assert(fork_height(stage, chainstate.headers()) == fh2);
        spdlog::debug("Drop all stage", fh1.value(), fh2.value() - 1);
        auto dk { db.schedule_protected_all() };
        blockCache.schedule_discard(dk);
    } else {
        newProtectBegin = fh2;
        spdlog::debug("Blocks already in stage: [{},{}], drop from {}", fh1.value(), fh2.value() - 1, fh2.value());
        auto dk { db.schedule_protected_part(stage, fh2) };
        blockCache.schedule_discard(dk);
    }

    stage = std::move(hc);
    const Height bound = stage.length();
    NonzeroHeight h(newProtectBegin->value());
    for (; h <= bound; ++h) {
        Hash hash = stage.hash_at(h);
        auto id { db.lookup_block_id(hash) };
        if (!id)
            break;
        db.protect_stage_assert_scheduled(*id);
    }
    t.commit();

    stage.shrink(h - 1);
    if (h > newProtectBegin) {
        spdlog::debug("MISSING: [{},{}), protected downloaded blocks [{},{}]", h.value(), l.value(), newProtectBegin->value(), (h - 1).value());
    } else {
        spdlog::debug("MISSING: [{},{}), protected no blocks", h.value(), l.value());
    }
    return { h };
}

auto State::add_stage(const std::vector<Block>& blocks, const Headerchain& hc) -> std::pair<stage_operation::StageAddResult, std::optional<StateUpdate>>
{
    if (signedSnapshot && !signedSnapshot->compatible(stage)) {
        return { { { ELEADERMISMATCH, signedSnapshot->height() } }, {} };
    }

    assert(blocks.size() > 0);
    ChainError err { Error(0), blocks.back().height + 1 };
    auto transaction = db.transaction();

    assert(hc.length() >= stage.length());
    assert(hc.hash_at(stage.length()) == stage.hash_at(stage.length()));
    for (auto& b : blocks) {
        assert(hc.length() >= b.height);
        assert(hc[b.height] == b.header);

        assert(b.height == stage.length() + 1);

        auto prepared { stage.prepare_append(signedSnapshot, b.header) };
        if (!prepared.has_value()) {
            err = { prepared.error(), b.height };
            break;
        }
        BodyView bv(b.body.view());
        if (b.header.merkleroot() != bv.merkleRoot()) {
            err = { EMROOT, b.height };
            break;
        }
        if (!bv.valid()) {
            err = { EMALFORMED, b.height };
            break;
        }
        db.insert_protect(b);
        stage.append(prepared.value(), batchRegistry);
    }
    if (stage.total_work() > chainstate.headers().total_work()) {
        auto [error, update] = apply_stage(std::move(transaction));
        if (error.is_error())
            return { { error }, update };
        else
            return { { err }, update };
    } else {
        transaction.commit();
        return { { err }, {} };
    }
};

RollbackResult State::rollback(const Height newlength) const
{
    spdlog::info("Rolling back chain");
    std::vector<TransferTxExchangeMessage> toMempool;
    assert(newlength < chainlength());
    Height beginHeight = newlength + 1;
    const PinFloor newPinFloor { beginHeight - 1 };
    Height endHeight(chainlength() + 1);

    // load ids
    auto ids { db.consensus_block_ids(beginHeight, endHeight) };
    assert(ids.size() == endHeight - beginHeight);
    std::optional<AccountId> oldAccountStart;
    std::map<AccountId, Funds> balanceMap;
    for (size_t i = 0; i < ids.size(); ++i) {
        const auto id { ids[i] };
        Height height = beginHeight + i;
        PinFloor pinFloor { height - 1 };
        auto u = db.get_block_undo(id);
        if (!u)
            throw std::runtime_error("Database corrupted (could not load block)");
        auto& [header, body, undo] = *u;

        BodyView bv(body);
        if (!bv.valid())
            throw std::runtime_error(
                "Database corrupted (invalid block body at height " + std::to_string(height) + ".");

        for (auto t : bv.transfers()) {
            PinHeight pinHeight = t.pinHeight(pinFloor);
            if (pinHeight <= newPinFloor) {
                // extract transaction to mempool
                auto toAddress { db.lookup_account(t.toAccountId())->address };
                toMempool.push_back(
                    TransferTxExchangeMessage(t, pinHeight, toAddress));
            }
        }

        // roll back state modifications
        RollbackView rbv(undo);
        if (i == 0) {
            oldAccountStart = rbv.getBeginNewAccounts();
        }
        const size_t N = rbv.nAccounts();
        for (size_t j = 0; j < N; ++j) {
            auto entry = rbv.accountBalance(j);
            const Funds bal { entry.balance() };
            const AccountId id { entry.id() };
            if (id < oldAccountStart) {
                balanceMap.try_emplace(id, bal);
            }
        }
    }
    db.delete_history_from((newlength + 1).nonzero_assert());
    db.delete_state_from(*oldAccountStart);
    auto dk { db.delete_consensus_from((newlength + 1).nonzero_assert()) };
    // write balances to db
    for (auto& p : balanceMap) {
        db.set_balance(p.first, p.second);
    }
    return chainserver::RollbackResult {
        .shrinkLength { newlength },
        .toMempool { std::move(toMempool) },
        .chainTxIds { db.fetch_tx_ids(newlength) },
        .deletionKey { dk }
    };
};

auto State::apply_stage(ChainDBTransaction&& t) -> std::pair<ChainError, std::optional<StateUpdate>>
{
    assert(!signedSnapshot || signedSnapshot->compatible(stage));
    assert(stage.total_work() > chainstate.headers().total_work());
    const NonzeroHeight fh { fork_height(chainstate.headers(), stage) }; // first different height

    chainserver::ApplyStageTransaction tr { *this, std::move(t) };
    tr.consider_rollback(fh - 1);
    auto error = tr.apply_stage_blocks();
    if (error) {
        if (global().conf.localDebug) {
            assert(0 == 1); // In local debug mode no errors should occurr (no bad actors)
        }
        for (auto h { error.height() }; h < stage.length(); ++h)
            db.delete_bad_block(stage.hash_at(h));
        stage.shrink(error.height() - 1);
        if (stage.total_work_at(error.height() - 1) <= chainstate.headers().total_work()) {
            return { error, {} };
        }
    }
    db.set_consensus_work(stage.total_work());
    auto update { tr.commit(*this) };

    spdlog::info("New chain length {}", chainlength().value());
    return { error, update };
};

auto State::apply_signed_snapshot(SignedSnapshot&& ssnew) -> std::optional<StateUpdate>
{
    if (signedSnapshot >= ssnew) {
        spdlog::info("SetSignedPin {} OLD ", ssnew.height().value());
        return {};
    }
    spdlog::info("SetSignedPin {} new", ssnew.height().value());
    signedSnapshot = std::move(ssnew);

    using namespace state_update;

    // consider chainstate
    state_update::StateUpdate res {
        .chainstateUpdate = state_update::RollbackData {
            .data {},
            .signedSnapshot { *signedSnapshot } },
        .mempoolUpdate {},
    };
    auto db_t { db.transaction() };
    if (!signedSnapshot->compatible(chainstate.headers())) {
        assert(signedSnapshot->height() <= chainlength());
        auto rb { rollback(signedSnapshot->height() - 1) };

        std::unique_lock<std::mutex> ul(chainstateMutex);
        auto headers_ptr { blockCache.add_old_chain(chainstate, rb.deletionKey) };

        res.chainstateUpdate = state_update::RollbackData {
            .data { state_update::RollbackData::Data {
                .rollback { chainstate.rollback(rb) },
                .prevChain { std::move(headers_ptr) },
            } },
            .signedSnapshot { *signedSnapshot }
        };
        res.mempoolUpdate = chainstate.pop_mempool_log();
    } else {
        assert(chainstate.pop_mempool_log().size() == 0);
    };

    db.set_consensus_work(chainstate.headers().total_work());
    db.set_signed_snapshot(*signedSnapshot);
    db_t.commit();

    return res;
}

auto State::append_mined_block(const Block& b) -> StateUpdate
{
    auto nextHeight { (chainlength() + 1).nonzero_assert() };
    if (nextHeight != b.height)
        throw Error(EMINEDDEPRECATED);
    BodyView bv(b.body.view());
    auto prepared { chainstate.prepare_append(signedSnapshot, b.header) };
    if (!prepared.has_value())
        throw Error(prepared.error());
    if (b.header.merkleroot() != bv.merkleRoot())
        throw Error(EMROOT);
    if (!bv.valid())
        throw Error(EMALFORMED);
    if (chainlength() + 1 != b.height)
        throw Error(EBADHEIGHT);

    const auto nextAccountId { db.next_state_id() };
    const auto nextHistoryId { db.next_history_id() };

    // do db transaction for new block
    auto transaction = db.transaction();

    auto [blockId, inserted] { db.insert_protect(b) };
    if (!inserted) {
        spdlog::error("Mined block is already in database. This is a bug.");
        throw Error(EMINEDDEPRECATED);
    }

    chainserver::BlockApplier e { db, chainstate.headers(), chainstate.txids(), false };
    e.apply_block(bv, nextHeight, blockId);
    db.set_consensus_work(chainstate.work_with_new_block());
    transaction.commit();

    std::unique_lock<std::mutex> ul(chainstateMutex);
    auto headerchainAppend = chainstate.append(Chainstate::AppendSingle {
        .signedSnapshot { signedSnapshot },
        .prepared { prepared.value() },
        .newTxIds { e.move_new_txids() },
        .newHistoryOffset = nextHistoryId,
        .newAccountOffset { nextAccountId } });
    ul.unlock();

    return {
        .chainstateUpdate { state_update::Append {
            headerchainAppend,
            try_sign_chainstate() } },
        .mempoolUpdate { chainstate.pop_mempool_log() }
    };
};

tl::expected<mempool::Log, Error> State::append_gentx(std::vector<uint8_t>&& data)
{
    try {
        Reader r(data);
        PaymentCreateMessage m { r };
        if (int32_t e = chainstate.insert_tx(m); e != 0) {
            Error err(e);
            spdlog::warn("Rejected new transaction: {}", err.strerror());
            return tl::make_unexpected(err);
        }
        spdlog::info("Added new transaction to mempool");
        return chainstate.pop_mempool_log();
    } catch (Error& e) {
        return tl::make_unexpected(e);
    }
};

std::optional<Hash> State::get_pin_hash(PinHeight pinHeight)
{
    if (pinHeight > chainlength())
        return {};
    if (pinHeight < chainlength().pin_bgin())
        return {};
    return chainstate.headers().hash_at(pinHeight);
};

API::Balance State::api_get_address(AddressView address)
{
    if (auto p = db.lookup_address(address); p) {
        return API::Balance {
            std::get<0>(*p),
            std::get<1>(*p)
        };
    } else {
        return API::Balance {
            AccountId { 0 },
            Funds { 0 }
        };
    }
}

auto State::insert_txs(const TxVec& txs) -> std::pair<std::vector<int32_t>, mempool::Log>
{
    std::vector<int32_t> res;
    res.reserve(txs.size());
    for (auto& tx : txs) {
        res.push_back(chainstate.insert_tx(tx));
    }
    return { res, chainstate.pop_mempool_log() };
};

API::Head State::api_get_head() const
{
    PinFloor pf { chainlength() };
    return API::Head {
        .signedSnapshot { signedSnapshot },
        .worksum { chainstate.headers().total_work() },
        .nextTarget { chainstate.headers().next_target() },
        .hash { chainstate.final_hash() },
        .height { chainlength() },
        .pinHash { chainstate.headers().hash_at(pf) },
        .pinHeight { pf },
    };
}

auto State::api_get_mempool(size_t) -> API::MempoolEntries
{
    std::vector<Hash> hashes;
    auto entries = chainstate.mempool().get_payments(100, &hashes);
    assert(hashes.size() == entries.size());
    API::MempoolEntries out;
    for (size_t i = 0; i < hashes.size(); ++i) {
        out.entries.push_back(API::MempoolEntry {
            entries[i], hashes[i] });
    }
    return out;
};

auto State::api_get_history(Address a, uint64_t beforeId) -> std::optional<API::History>
{
    auto p = db.lookup_address(a);
    if (!p)
        return {};
    auto& [accountId, balance] = *p;

    std::vector entries_desc = db.lookup_history_100_desc(accountId, beforeId);
    std::vector<API::Block> blocks_reversed;
    PinFloor pinFloor { 0 };
    uint64_t firstHistoryId = 0;
    uint64_t nextHistoryOffset = 0;
    chainserver::AccountCache cache(db);

    uint64_t prevHistoryId = 0;
    for (auto& [historyId, txid, data] : std::ranges::reverse_view { entries_desc }) {
        if (firstHistoryId == 0)
            firstHistoryId = historyId;
        assert(prevHistoryId < historyId);
        prevHistoryId = historyId;
        if (historyId >= nextHistoryOffset) {
            auto height { chainstate.history_height(historyId) };
            pinFloor = PinFloor(height - 1);
            auto header = chainstate.headers()[height];
            bool b = height == chainlength();
            nextHistoryOffset = (b
                    ? std::numeric_limits<uint64_t>::max()
                    : chainstate.historyOffset(height + 1));
            blocks_reversed.push_back(
                API::Block(header, height, 1 + (chainlength() - height)));
        }
        API::Block& b = blocks_reversed.back();
        b.push_history(txid, data, cache, pinFloor);
    }

    return API::History {
        .balance = balance,
        .fromId = firstHistoryId,
        .blocks_reversed = blocks_reversed
    };
};

auto State::api_get_richlist(size_t N) -> API::Richlist
{
    return db.lookup_richlist(N);
};

auto State::get_blocks(DescriptedBlockRange range) -> std::vector<BodyContainer>
{
    assert(range.lower != 0);
    assert(range.upper >= range.lower);
    std::vector<Hash> hashes(range.upper - range.lower + 1);
    std::vector<BodyContainer> res;
    if (range.descriptor == chainstate.descriptor()) {
        if (chainstate.length() < range.upper)
            return {};
        for (Height h = range.lower; h < range.upper + 1; ++h) {
            hashes[h - range.lower] = chainstate.headers().hash_at(h);
        }
    } else {
        hashes = blockCache.get_hashes(range);
    }
    for (size_t i = 0; i < hashes.size(); ++i) {
        auto hash { hashes[i] };
        auto b { db.get_block(hash) };
        if (b) {
            res.push_back(std::move(b->second.body));
        } else {
            spdlog::error("BUG: no block with hash {} in db.", serialize_hex(hash));
            return {};
        }
    }
    return res;
};

auto State::get_mempool_tx(TransactionId txid) const -> std::optional<TransferTxExchangeMessage>
{
    return chainstate.mempool()[txid];
};

auto State::commit_fork(RollbackResult&& rr, AppendBlocksResult&& abr) -> StateUpdate
{
    assert(!signedSnapshot || signedSnapshot->compatible(stage));
    auto forkHeight { (rr.shrinkLength + 1).nonzero_assert() };
    auto headers_ptr { blockCache.add_old_chain(chainstate, rr.deletionKey) };
    chainstate.fork(chainserver::Chainstate::ForkData {
        .stage { stage },
        .rollbackResult { std::move(rr) },
        .appendResult { std::move(abr) },
    });

    state_update::Fork forkMsg {
        chainstate.headers().get_fork(forkHeight, chainstate.descriptor()),
        std::move(headers_ptr),
        try_sign_chainstate()
    };

    return StateUpdate {
        .chainstateUpdate { std::move(forkMsg) },
        .mempoolUpdate { chainstate.pop_mempool_log() },
    };
}

auto State::commit_append(AppendBlocksResult&& abr) -> StateUpdate
{
    assert(!signedSnapshot || signedSnapshot->compatible(stage));
    auto headerchainAppend { chainstate.append(Chainstate::AppendMulti {
        .patchedChain = stage,
        .appendResult { std::move(abr) },
    }) };

    return {
        .chainstateUpdate {
            state_update::Append {
                headerchainAppend,
                try_sign_chainstate(),
            } },
        .mempoolUpdate { chainstate.pop_mempool_log() }
    };
}

std::optional<SignedSnapshot> State::try_sign_chainstate()
{
    if ((!signedSnapshot.has_value() || (signedSnapshot->height() < chainstate.length()))
        && (signAfter < std::chrono::steady_clock::now() && signingEnabled)
        && snapshotSigner.has_value()) {
        spdlog::info("Signing chain state at height {}", chainstate.length().value());
        signedSnapshot = snapshotSigner->sign(chainstate);
        db.set_signed_snapshot(*signedSnapshot);
        return signedSnapshot;
    }
    return {};
};
}
