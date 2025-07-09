#include "helpers/cache.hpp"
#ifndef DISABLE_LIBUV
#include "api/http/endpoint.hpp"
#endif

#include "../db/chain_db.hpp"
#include "api/types/all.hpp"
#include "block/body/rollback.hpp"
#include "block/body/view.hpp"
#include "block/chain/history/history.hpp"
#include "block/header/header_impl.hpp"
#include "communication/create_transaction.hpp"
#include "eventloop/types/chainstate.hpp"
#include "general/hex.hpp"
#include "general/is_testnet.hpp"
#include "global/globals.hpp"
#include "spdlog/spdlog.h"
#include "state.hpp"
#include "transactions/apply_stage.hpp"
#include "transactions/block_applier.hpp"
#include <ranges>

namespace chainserver {

void MiningCache::update_validity(CacheValidity cv)
{
    if (cacheValidity != cv)
        cache.clear();
    cacheValidity = cv;
}
const VersionedBodyContainer* MiningCache::lookup(const Address& a, bool disableTxs) const
{
    auto iter { std::find_if(cache.begin(), cache.end(), [&](const Item& i) {
        return i.address == a && i.disableTxs == disableTxs;
    }) };
    if (iter != cache.end())
        return &iter->b;
    return nullptr;
}

const BodyContainer& MiningCache::insert(const Address& a, bool disableTxs, VersionedBodyContainer b)
{
    cache.push_back({ a, disableTxs, std::move(b) });
    return cache.back().b;
}

State::State(ChainDB& db, BatchRegistry& br, std::optional<SnapshotSigner> snapshotSigner)
    : db(db)
    , dbcache(db)
    , batchRegistry(br)
    , snapshotSigner(std::move(snapshotSigner))
    , signedSnapshot(db.get_signed_snapshot())
    , chainstate(db, br)
    , nextGarbageCollect(std::chrono::steady_clock::now())
    , _miningCache(mining_cache_validity())
{
}

std::optional<std::pair<NonzeroHeight, Header>> State::get_header(Height h) const
{
    if (auto p { chainstate.headers().get_header(h) }; p.has_value())
        return std::pair<NonzeroHeight, Header> { h.nonzero_assert(), Header(p.value()) };
    return {};
}

auto State::api_get_header(api::HeightOrHash& hh) const -> std::optional<std::pair<NonzeroHeight, Header>>
{
    if (std::holds_alternative<Height>(hh.data)) {
        return get_header(std::get<Height>(hh.data));
    }
    auto h { consensus_height(std::get<Hash>(hh.data)) };
    if (!h.has_value())
        return {};
    return get_header(*h);
}

std::optional<NonzeroHeight> State::consensus_height(const Hash& hash) const
{
    auto o { db.lookup_block_height(hash) };
    if (!o.has_value())
        return {};
    auto& h { o.value() };
    auto hash2 { chainstate.headers().get_hash(h) };
    if (!hash2.has_value() || *hash2 != hash)
        return {};
    return h;
}

std::optional<Hash> State::get_hash(Height h) const
{
    return chainstate.headers().get_hash(h);
}

std::optional<api::Block> State::api_get_block(const api::HeightOrHash& hh) const
{
    if (std::holds_alternative<Height>(hh.data)) {
        return api_get_block(std::get<Height>(hh.data));
    }
    auto h { consensus_height(std::get<Hash>(hh.data)) };
    if (!h.has_value())
        return {};
    return api_get_block(*h);
}
namespace {

void push_history(api::Block& b, const history::Entry& e, chainserver::DBCache& c,
    PinFloor pinFloor)
{
    e.data.visit_overload(
        [&](const history::WartTransferData& d) {
            b.actions.wartTransfers.push_back(
                api::block::WartTransfer {
                    .txhash = e.hash,
                    .fromAddress = c.addresses.fetch(d.origin_account_id()),
                    .fee = d.fee(),
                    .nonceId = d.pin_nonce().id,
                    .pinHeight = d.pin_nonce().pin_height_from_floored(pinFloor),
                    .toAddress = c.addresses.fetch(d.to_id()),
                    .amount = d.wart() });
        },
        [&](const history::RewardData& d) {
            auto toAddress = c.addresses.fetch(d.to_id());
            b.set_reward({ e.hash, toAddress, d.wart() });
        },
        [&](const history::AssetCreationData& d) {
            b.actions.assetCreations.push_back(
                { .txhash { e.hash },
                    .assetName { d.asset_name() },
                    .supply { d.supply() },
                    .assetId { d.asset_id() },
                    .fee { d.fee() } });
        },
        [&](const history::TokenTransferData& d) {
            auto& assetData { c.assetsById[d.token_id().corresponding_asset_id()] };

            b.actions.tokenTransfers.push_back(
                api::block::TokenTransfer {
                    .txhash = e.hash,
                    .fromAddress = c.addresses.fetch(d.origin_account_id()),
                    .fee = d.fee(),
                    .nonceId = d.pin_nonce().id,
                    .pinHeight = d.pin_nonce().pin_height_from_floored(pinFloor),
                    .toAddress = c.addresses.fetch(d.to_id()),
                    .amount = { d.amount() },
                    .assetInfo = assetData });
        },
        [&](const history::OrderData& d) {
            auto& assetData { c.assetsById[d.asset_id()] };
            b.actions.newOrders.push_back(api::block::NewOrder { .txhash { e.hash },
                .assetInfo { assetData.id_hash_name_precision() },
                .fee { d.fee() },
                .amount { d.amount() },
                .limit { d.limit() },
                .buy = d.buy(),
                .address { c.addresses.fetch(d.account_id()) } });
        },
        [&](const history::CancelationData& d) {
            b.actions.cancelations.push_back({
                .txhash { e.hash },
                .fee { d.fee() },
                .address { c.addresses.fetch(d.cancel_account_id()) },
            });
        },
        [&](const history::MatchData& d) {
            auto& asset { c.assetsById[d.asset_id()] };
            b.actions.matches.push_back(api::block::Match { .txhash { e.hash },
                .assetInfo { asset.id_hash_name_precision() },
                .liquidityBefore { d.pool_before() },
                .liquidityAfter { d.pool_after() },
                .buySwaps {},
                .sellSwaps {} });
        },
        [&](const history::LiquidityDeposit& ld) {
            b.actions.liquidityDeposit.push_back(
                { .txhash { e.hash },
                    .fee { ld.fee() },
                    .baseDeposited { ld.base() },
                    .quoteDeposited { ld.quote() },
                    .sharesReceived { ld.shares() } });
        },
        [&](const history::LiquidityWithdraw& lw) {
            b.actions.liquidityWithdrawal.push_back({
                .txhash { e.hash },
                .fee { lw.fee() },
                .sharesRedeemed { lw.shares() },
                .baseReceived { lw.base() },
                .quoteReceived { lw.quote() },
            });
        });
}
}

std::optional<api::Block> State::api_get_block(Height zh) const
{
    if (zh == 0 || zh > chainlength())
        return {};
    auto h { zh.nonzero_assert() };
    auto pinFloor { h.pin_floor() };
    auto lower = chainstate.historyOffset(h);
    auto upper = (h == chainlength() ? HistoryId { 0 }
                                     : chainstate.historyOffset(h + 1));
    auto header = chainstate.headers()[h];

    auto entries { db.lookup_history_range(lower, upper) };
    api::Block b(header, h, chainlength() - h + 1, {});
    for (auto& e : entries)
        push_history(b, e, dbcache, pinFloor);
    return b;
}

auto State::api_tx_cache() const -> const TransactionIds
{
    return chainstate.txids();
}

api::Transaction State::api_dispatch_mempool(const TxHash& txHash, TransactionMessage&& tx) const
{
    auto gen_temporal = []() { return api::TemporalInfo { 0, Height(0), 0 }; };
    return std::move(tx).visit_overload(
        [&](WartTransferMessage&& wtm) -> api::Transaction {
            return api::WartTransferTransaction {
                gen_temporal(), {
                                    .txhash = txHash,
                                    .fromAddress = wtm.from_address(txHash),
                                    .fee = wtm.fee(),
                                    .nonceId = wtm.nonce_id(),
                                    .pinHeight = wtm.pin_height(),
                                    .toAddress = wtm.to_addr(),
                                    .amount = wtm.wart(),
                                }
            };
        },
        [&](TokenTransferMessage&& ttm) -> api::Transaction {
            // ttm.byte_size
            auto& a { dbcache.assetsByHash[ttm.asset_hash()] };
            return api::TokenTransferTransaction {
                gen_temporal(), { // AssetIdHashNamePrecision assetInfo;
                                    .txhash = txHash,
                                    .fromAddress = ttm.from_address(txHash),
                                    .fee = ttm.fee(),
                                    .nonceId = ttm.nonce_id(),
                                    .pinHeight = ttm.pin_height(),
                                    .toAddress = ttm.to_addr(),
                                    .amount = ttm.amount(),
                                    .assetInfo { a.id_hash_name_precision() } }
            };
        },
        [&](OrderMessage&& o) -> api::Transaction {
            auto& a { dbcache.assetsByHash[o.asset_hash()] };
            return api::NewOrderTransaction {
                gen_temporal(), {

                                    .txhash = txHash,
                                    .assetInfo { a.id_hash_name_precision() },
                                    .fee { o.fee() },
                                    .amount { o.amount() },
                                    .limit { o.limit() },
                                    .buy = o.buy(),
                                    .address { o.from_address(txHash) },
                                }
            };
        },
        [&](CancelationMessage&& a) -> api::Transaction {
            return api::CancelationTransaction {
                gen_temporal(),
                {
                    .txhash { txHash },
                    .fee { a.fee() },
                    .address { a.from_address(txHash) },
                }
            };
        },
        [&](LiquidityAddMessage&& rd) -> api::Transaction {
            return api::LiquidityDepositTransaction {
                gen_temporal(), { .txhash { txHash }, .fee { rd.fee() }, .baseDeposited { rd.amount() }, .quoteDeposited { rd.wart() }, .sharesReceived { std::nullopt } }
            };
        },
        [&](LiquidityRemoveMessage&& rm) -> api::Transaction {
            return api::LiquidityWithdrawalTransaction {
                gen_temporal(), { .txhash { txHash }, .fee { rm.fee() }, .sharesRedeemed { rm.amount() }, .baseReceived { std::nullopt }, .quoteReceived { std::nullopt } }
            };
        },
        [&](AssetCreationMessage&& rm) -> api::Transaction {
            return api::AssetCreationTransaction {
                gen_temporal(), {
                                    .txhash { txHash },
                                    .assetName { rm.asset_name() },
                                    .supply { rm.supply() },
                                    .assetId { std::nullopt },
                                    .fee { rm.fee() },
                                }
            };
        });
}

api::Transaction State::api_dispatch_history(const TxHash& txHash, history::HistoryVariant&& tx, NonzeroHeight h) const
{
    auto gen_temporal = [&]() { return api::TemporalInfo { (chainlength() + 1) - h, h, get_headers()[h].timestamp() }; };
    auto fetch_addr { [&](AccountId aid) {
        return dbcache.addresses.fetch(aid);
    } };
    return std::move(tx).visit_overload(
        [&](history::WartTransferData&& wtm) -> api::Transaction {
            return api::WartTransferTransaction {
                gen_temporal(), {
                                    .txhash = txHash,
                                    .fromAddress = fetch_addr(wtm.origin_account_id()),
                                    .fee = wtm.fee(),
                                    .nonceId = wtm.pin_nonce().id,
                                    .pinHeight = wtm.pin_nonce().pin_height_from_floored(h.pin_floor()),
                                    .toAddress = fetch_addr(wtm.to_id()),
                                    .amount = wtm.wart(),
                                }
            };
        },
        [&](history::TokenTransferData&& ttm) -> api::Transaction {
            auto& a { dbcache.assetsById[ttm.token_id().corresponding_asset_id()] };
            return api::TokenTransferTransaction {
                gen_temporal(), { // AssetIdHashNamePrecision assetInfo;
                                    .txhash = txHash,
                                    .fromAddress = fetch_addr(ttm.origin_account_id()),
                                    .fee = ttm.fee(),
                                    .nonceId = ttm.pin_nonce().id,
                                    .pinHeight = ttm.pin_nonce().pin_height_from_floored(h.pin_floor()),
                                    .toAddress = fetch_addr(ttm.to_id()),
                                    .amount = ttm.amount(),
                                    .assetInfo { a.id_hash_name_precision() } }
            };
        },
        [&](history::OrderData&& o) -> api::Transaction {
            auto& a { dbcache.assetsById[o.asset_id()] };
            return api::NewOrderTransaction {
                gen_temporal(), {

                                    .txhash = txHash,
                                    .assetInfo { a.id_hash_name_precision() },
                                    .fee { o.fee() },
                                    .amount { o.amount() },
                                    .limit { o.limit() },
                                    .buy = o.buy(),
                                    .address { fetch_addr(o.account_id()) },
                                }
            };
        },
        [&](history::CancelationData&& a) -> api::Transaction {
            return api::CancelationTransaction {
                gen_temporal(),
                {
                    .txhash { txHash },
                    .fee { a.fee() },
                    .address { fetch_addr(a.cancel_account_id()) },
                }
            };
        },
        [&](history::LiquidityDeposit&& rd) -> api::Transaction {
            return api::LiquidityDepositTransaction {
                gen_temporal(), { .txhash { txHash }, .fee { rd.fee() }, .baseDeposited { rd.base() }, .quoteDeposited { rd.quote() }, .sharesReceived { rd.shares() } }
            };
        },
        [&](history::LiquidityWithdraw&& rm) -> api::Transaction {
            return api::LiquidityWithdrawalTransaction {
                gen_temporal(), { .txhash { txHash }, .fee { rm.fee() }, .sharesRedeemed { rm.shares() }, .baseReceived { rm.base() }, .quoteReceived { rm.quote() } }
            };
        },
        [&](history::RewardData&& rm) -> api::Transaction {
            return api::RewardTransaction {
                gen_temporal(), { .txhash { txHash }, .toAddress { fetch_addr(rm.to_id()) }, .amount { rm.wart() } }
            };
        },
        [&](history::AssetCreationData&& rm) -> api::Transaction {
            return api::AssetCreationTransaction {
                gen_temporal(), {
                                    .txhash { txHash },
                                    .assetName { rm.asset_name() },
                                    .supply { rm.supply() },
                                    .assetId { rm.asset_id() },
                                    .fee { rm.fee() },
                                }
            };
        },
        [&](history::MatchData&& rm) -> api::Transaction {
            auto& a { dbcache.assetsById[rm.asset_id()] };
            return api::MatchTransaction {
                gen_temporal(), {
                                    .txhash { txHash },
                                    .assetInfo { a.id_hash_name_precision() },
                                    .liquidityBefore { rm.pool_before() },
                                    .liquidityAfter { rm.pool_after() },
                                    .buySwaps { rm.buy_swaps() },
                                    .sellSwaps { rm.sell_swaps() },
                                }
            };
            // AssetIdEl, PoolBeforeEl, PoolAfterEl, BuySwapsEl, SellSwapsEl
        }

    );
}

std::optional<api::Transaction> State::api_get_tx(const TxHash& txHash) const
{
    if (auto p { chainstate.mempool()[txHash] }; p)
        return api_dispatch_mempool(txHash, std::move(*p));
    if (auto p { db.lookup_history(txHash) }; p) {
        auto& [parsed, historyIndex] = *p;
        NonzeroHeight h { chainstate.history_height(historyIndex) };
        return api_dispatch_history(txHash, std::move(parsed), h);
    }
    return {};
}

auto State::api_get_latest_txs(size_t N) const -> api::TransactionsByBlocks
{
    HistoryId upper { db.next_history_id() };
    // note: history ids start with 1
    HistoryId lower { (upper.value() > N + 1) ? db.next_history_id() - N : HistoryId { 1 } };
    return api_get_transaction_range(lower, upper);
}

auto State::api_get_latest_blocks(size_t N) const -> api::TransactionsByBlocks
{
    HistoryId upper { db.next_history_id() };
    auto l { chainlength().value() };
    auto hLower { l > N ? Height(l + 1 - N).nonzero_assert() : Height { 1 }.nonzero_assert() };
    HistoryId lower { chainstate.historyOffset(hLower) };
    return api_get_transaction_range(lower, upper);
}

auto State::api_get_miner(NonzeroHeight h) const -> std::optional<api::AddressWithId>
{
    if (chainlength() < h)
        return {};
    auto offset { chainstate.historyOffset(h) };
    auto parsed { db.fetch_history(offset).data };

    assert(std::holds_alternative<history::RewardData>(parsed));
    auto minerId { std::get<history::RewardData>(parsed).to_id() };
    return api::AddressWithId {
        db.fetch_address(minerId),
        minerId
    };
}

auto State::api_get_miners(HeightRange hr) const -> std::vector<api::AddressWithId>
{
    std::vector<api::AddressWithId> res;
    for (auto h : hr) {
        auto o { api_get_miner(h) };
        assert(o);
        res.push_back(*o);
    }
    return res;
}

auto State::api_get_latest_miners(uint32_t N) const -> std::vector<api::AddressWithId>
{
    return api_get_miners(chainlength().latest(N));
}

auto State::api_get_transaction_range(HistoryId lower, HistoryId upper) const -> api::TransactionsByBlocks
{
    api::TransactionsByBlocks res { .fromId { lower }, .blocks_reversed {} };
    if (upper.value() == 0)
        return res;
    auto lookup { db.lookup_history_range(lower, upper) };
    assert(lookup.size() == upper - lower);
    if (chainlength() != 0) {
        chainserver::DBCache cache(db);
        auto update_tmp = [&](HistoryId id) {
            auto h { chainstate.history_height(id) };
            auto pinFloor { h.pin_floor() };
            auto header { chainstate.headers()[h] };
            auto b { api::Block(header, h, chainlength() - h + 1, {}) };
            auto beginId { chainstate.historyOffset(h) };
            return std::tuple { pinFloor, beginId, b };
        };
        auto tmp { update_tmp(upper - 1) };

        for (size_t i = 0; i < lookup.size(); ++i) {
            auto& [pinFloor, beginId, block] { tmp };
            auto id { upper - 1 - i };
            if (id < beginId) { // start new tmp block
                res.blocks_reversed.push_back(block);
                tmp = update_tmp(id);
            }

            push_history(block, lookup[lookup.size() - 1 - i], cache, pinFloor);
        }
        res.count = lookup.size();
        res.blocks_reversed.push_back(std::get<2>(tmp));
    }
    return res;
}

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

Batch State::get_headers_concurrent(BatchSelector s) const
{
    std::unique_lock<std::mutex> lcons(chainstateMutex);
    if (s.descriptor == chainstate.descriptor()) {
        return chainstate.headers().get_headers(s.header_range());
    } else {
        return blockCache.get_batch(s);
    }
}

std::optional<HeaderView> State::get_header_concurrent(Descriptor descriptor, Height height) const
{
    std::unique_lock<std::mutex> lcons(chainstateMutex);
    if (descriptor == chainstate.descriptor()) {
        return chainstate.headers().get_header(height);
    } else {
        return blockCache.get_header(descriptor, height);
    }
}

ConsensusSlave State::get_chainstate_concurrent()
{
    std::unique_lock<std::mutex> l(chainstateMutex);
    return { signedSnapshot, chainstate.descriptor(), chainstate.headers() };
}

Result<ChainMiningTask> State::mining_task(const Address& a)
{
    return mining_task(a, config().node.disableTxsMining);
}

Result<ChainMiningTask> State::mining_task(const Address& miner, bool disableTxs)
{
    auto md = chainstate.mining_data();

    NonzeroHeight height { next_height() };
    if (height.value() < NEWBLOCKSTRUCUTREHEIGHT && !is_testnet())
        return Error(ENOTSYNCED);

    auto make_body {
        [&]() -> block::Body {
            std::vector<TransactionMessage> transactions;
            if (!disableTxs)
                transactions = chainstate.mempool().get_transactions(400, height);

            auto minerReward { height.reward() };

            using namespace block;

            std::vector<Address> newAddresses;
            auto addr_id {
                [&, map = std::map<Address, AccountId> {}, nextAccountId = db.next_account_id()](const Address& address) mutable -> AccountId {
                    auto a { db.lookup_account(address) };
                    if (a)
                        return a.value();
                    auto [iter, inserted] { map.try_emplace(address, nextAccountId) };
                    if (inserted) {
                        nextAccountId++;
                        newAddresses.push_back(address);
                    }
                    return iter->second;
                }
            };
            const AccountId minerAccId { addr_id(miner) };
            body::Entries entries;
            auto asset {
                [&, assetOffsets = std::map<AssetHash, size_t> {}](AssetHash hash) mutable -> auto& {
                    auto [it, inserted] = assetOffsets.try_emplace(hash, entries.tokens().size());
                    if (inserted)
                        entries.tokens().push_back({ dbcache.assetsByHash[hash].id });
                    return entries.tokens()[it->second];
                }
            };

            for (auto& tx : transactions) {
                minerReward.add_assert(tx.fee()); // assert because
                std::move(tx).visit_overload(
                    [&](WartTransferMessage&& m) {
                        entries.wart_transfers().push_back({ m.from_id(), m.pin_nonce_throw(height), m.compact_fee(), addr_id(m.to_addr()), m.wart(), m.signature() });
                    },
                    [&](TokenTransferMessage&& m) {
                        auto pn { PinNonce::make_pin_nonce(m.nonce_id(), height, m.pin_height()) };
                        if (!pn)
                            throw std::runtime_error("Cannot make pin_nonce");
                        auto& s { asset(m.asset_hash()) };
                        auto& transfers = s.asset_transfers();
                        transfers.push_back({ m.from_id(), m.pin_nonce_throw(height), m.compact_fee(), addr_id(m.to_addr()), m.amount(), m.signature() });
                    },
                    [&](OrderMessage&& m) {
                        asset(m.asset_hash())
                            .orders.push_back({ m.from_id(), m.pin_nonce_throw(height), m.compact_fee(), m.buy(), m.amount(), m.limit(), m.signature() });
                    },
                    [&](CancelationMessage&& m) {
                        entries
                            .cancelations.push_back({ m.from_id(), m.pin_nonce_throw(height), m.compact_fee(), PinNonce(m.block_pin_nonce()), m.signature() });
                    },
                    [&](LiquidityAddMessage&& m) {
                        asset(m.asset_hash())
                            .liquidityAdd.push_back({ m.from_id(), m.pin_nonce_throw(height), m.compact_fee(), m.wart(), m.amount(), m.signature() });
                    },
                    [&](LiquidityRemoveMessage&& m) {
                        asset(m.asset_hash())
                            .liquidityRemove.push_back({ m.from_id(), m.pin_nonce_throw(height), m.compact_fee(), m.amount(), m.signature() });
                    },
                    [&](AssetCreationMessage&& m) {
                        entries.assetCreations
                            .push_back({ m.from_id(), m.pin_nonce_throw(height), m.compact_fee(), AssetSupplyEl(m.supply()), m.asset_name(), m.signature() });
                    });
            }
            return { std::move(newAddresses), { minerAccId, minerReward }, std::move(entries) };
        }
    };

    const auto b {
        [&]() -> const VersionedBodyContainer& {
            _miningCache.update_validity(mining_cache_validity());
            if (auto* p { _miningCache.lookup(miner, disableTxs) }; p != nullptr) {
                return *p;
            } else {
                auto body { make_body() };
                auto& b { _miningCache.insert(miner, disableTxs, std::move(body)) };
                return b;
            }
        }()
    };

    try {
        auto structure { block::body::Structure::parse_throw(b, height, b.version) };
        BodyView bv(b, structure);

        HeaderGenerator hg(md.prevhash, bv, md.target, md.timestamp, height);
        return ChainMiningTask { ParsedBlock::create_throw(height, hg.make_header(0), std::move(b)) };
    } catch (const Error& e) {
        spdlog::warn("Cannot create mining task: {}", e.strerror());
        return Error(EBUG);
    }
}

stage_operation::StageSetStatus State::set_stage(Headerchain&& hc)
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

auto State::add_stage(const std::vector<ParsedBlock>& blocks, const Headerchain& hc) -> StageActionResult
{
    if (signedSnapshot && !signedSnapshot->compatible(stage)) {
        return { { { ELEADERMISMATCH, signedSnapshot->height() } }, {}, {} };
    }

    assert(blocks.size() > 0);
    ChainError ce { Error(0), blocks.back().height + 1 };
    auto transaction = db.transaction();

    assert(hc.length() >= stage.length());
    assert(hc.hash_at(stage.length()) == stage.hash_at(stage.length()));
    for (auto& b : blocks) {
        assert(hc.length() >= b.height);
        assert(hc[b.height] == b.header);
        assert(b.height == stage.length() + 1);

        auto prepared { stage.prepare_append(signedSnapshot, b.header) };
        if (!prepared.has_value()) {
            ce = { prepared.error(), b.height };
            break;
        }
        db.insert_protect(b);
        stage.append(prepared.value(), batchRegistry);
    }
    if (stage.total_work() > chainstate.headers().total_work()) {
        auto [status, update] { apply_stage(std::move(transaction)) };

        if (status.ce.is_error()) {
            // Something went wrong on block body level so block header must be also tainted
            // as we checked for correct merkleroot already
            // => we need to collect data on rogue header
            RogueHeaderData rogueHeaderData(
                status.ce,
                stage[status.ce.height()],
                stage.total_work_at(status.ce.height()));
            return { { status }, rogueHeaderData, update };
        } else {
            // pass {} as header arg because we can't to block any headers when
            // we have a wrong body (EINV_BODY or EMROOT)
            return { { ce }, {}, update };
        }
    } else {
        // pass {} as header arg because we can't to block any headers when
        // we have a wrong body (EINV_BODY or EMROOT)
        transaction.commit();
        dbcache.clear();
        return { { ce }, {}, {} };
    }
}
namespace {
class RollbackSession {
public:
    const ChainDB& db;
    const PinFloor newPinFloor;
    const AccountId oldAccountStart;
    const TokenId oldTokenStart;

    std::map<AccountToken, Funds_uint64> balanceMap;
    std::vector<WartTransferMessage> toMempool;
    std::optional<BalanceId> oldBalanceStart;

private:
    RollbackSession(const ChainDB& db, NonzeroHeight beginHeight,
        const rollback::Data& rb)
        : db(db)
        , newPinFloor(beginHeight.pin_floor())
        , oldAccountStart(rb.next_account_id())
        , oldTokenStart(rb.next_token_id())
    {
    }

    static BlockUndoData fetch_undo(const ChainDB& db, BlockId id)
    {
        auto u = db.get_block_undo(id);
        if (!u)
            throw std::runtime_error("Database corrupted (could not load block)");
        return *u;
    }

public:
    RollbackSession(const ChainDB& db, NonzeroHeight beginHeight, BlockId firstId)
        : RollbackSession(db, beginHeight, rollback::Data(fetch_undo(db, firstId).rawUndo))
    {
    }

    void rollback_block(BlockId id, NonzeroHeight height, AddressCache& ac)
    {
        try {
            BlockUndoData d { fetch_undo(db, id) };
            auto pinFloor { height.pin_floor() };
            auto s { d.body.parse_structure_throw(height, d.header.version()) };
            BodyView bv { d.body, s };
            for (auto t : bv.wart_transfers()) {
                PinHeight pinHeight = t.pin_height(pinFloor);
                if (pinHeight <= newPinFloor) {
                    // extract transaction to mempool
                    auto toAddress { ac.fetch(t.toAccountId()) };
                    toMempool.push_back(WartTransferMessage(t, pinHeight, toAddress));
                }
            }

            // roll back state modifications
            rollback::Data rbv(d.rawUndo);
            rbv.foreach_balance_update(
                [&](const AccountTokenBalance& entry) {
                    const Funds_uint64& bal { entry.balance };
                    const BalanceId& id { entry.id };
                    auto b { db.get_token_balance(id) };
                    if (!b.has_value())
                        throw std::runtime_error("Database corrupted, cannot roll back");
                    AccountToken at { b->accountId, b->tokenId };
                    balanceMap.try_emplace(at, bal);
                });
        } catch (const Error& e) {
            throw std::runtime_error(
                "Cannot rollback block at height" + std::to_string(height) + ":" + e.err_name());
        }
    }
};
}

RollbackResult
State::rollback(const Height newlength) const
{
    const Height oldlength { chainlength() };
    spdlog::info("Rolling back chain");
    assert(newlength < chainlength());
    NonzeroHeight beginHeight = (newlength + 1).nonzero_assert();
    auto endHeight(chainlength().add1());

    // load ids
    auto ids { db.consensus_block_ids({ beginHeight, endHeight }) };
    assert(ids.size() == endHeight - beginHeight);
    assert(ids.size() > 0);

    RollbackSession rs(db, beginHeight, ids[0]);

    for (size_t i = 0; i < ids.size(); ++i) {
        NonzeroHeight height = beginHeight + i;
        rs.rollback_block(ids[i], height);
    }

    db.delete_history_from(newlength.add1());
    db.delete_state_from(rs.oldAccountStart);
    auto dk { db.delete_consensus_from((newlength + 1).nonzero_assert()) };

    // write balances to db
    for (auto& p : rs.balanceMap) {
        db.set_balance(p.first, p.second);
    }
    return chainserver::RollbackResult {
        .shrink { newlength, oldlength - newlength },
        .toMempool { std::move(rs.toMempool) },
        .wartUpdates { std::move(rs.balanceMap) },
        .chainTxIds { db.fetch_tx_ids(newlength) },
        .deletionKey { dk }
    };
}

auto State::apply_stage(ChainDBTransaction&& t) -> ApplyStageResult
{
    dbCacheValidity += 1;
    assert(!signedSnapshot || signedSnapshot->compatible(stage));
    assert(stage.total_work() > chainstate.headers().total_work());
    const NonzeroHeight fh { fork_height(chainstate.headers(), stage) }; // first different height

    chainserver::ApplyStageTransaction tr { *this, std::move(t) };
    tr.consider_rollback(fh - 1);
    auto status { tr.apply_stage_blocks() };
    if (status.is_error()) {
        if (config().localDebug) {
            assert(0 == 1); // In local debug mode no errors should occurr (no bad actors)
        }
        for (auto h { status.height() }; h < stage.length(); ++h)
            db.delete_bad_block(stage.hash_at(h));
        stage.shrink(status.height() - 1);
        if (stage.total_work_at(status.height() - 1) <= chainstate.headers().total_work()) {
            return {
                { status },
                {},
            };
        }
    }
    db.set_consensus_work(stage.total_work());
    auto update { std::move(tr).commit(*this) };
    dbcache.clear();

    return { { status }, update };
}

auto State::apply_signed_snapshot(SignedSnapshot&& ssnew) -> std::optional<StateUpdateWithAPIBlocks>
{
    if (signedSnapshot >= ssnew) {
        return {};
    }
    dbCacheValidity += 1;
    syncdebug_log().info("SetSignedPin {} new", ssnew.height().value());
    signedSnapshot = std::move(ssnew);

    using namespace state_update;

    // consider chainstate
    state_update::StateUpdateWithAPIBlocks res {
        .update {
            .chainstateUpdate = state_update::SignedSnapshotApply {
                .rollback {},
                .signedSnapshot { *signedSnapshot } },
            .mempoolUpdates {},
        },
        .appendedBlocks {}
    };
    auto db_t { db.transaction() };
    if (!signedSnapshot->compatible(chainstate.headers())) {
        assert(signedSnapshot->height() <= chainlength());
        auto rb { rollback(signedSnapshot->height() - 1) };

        std::unique_lock<std::mutex> ul(chainstateMutex);
        auto headers_ptr { blockCache.add_old_chain(chainstate, rb.deletionKey) };

        res.update.chainstateUpdate = state_update::SignedSnapshotApply {
            .rollback { state_update::SignedSnapshotApply::Rollback {
                .deltaHeaders { chainstate.rollback(rb) },
                .prevHeaders { std::move(headers_ptr) },
            } },
            .signedSnapshot { *signedSnapshot }
        };
        res.update.mempoolUpdates = chainstate.pop_mempool_updates();
    } else {
        assert(chainstate.pop_mempool_updates().size() == 0);
    };

    db.set_consensus_work(chainstate.headers().total_work());
    db.set_signed_snapshot(*signedSnapshot);
    db_t.commit();
    dbcache.clear();

    return res;
}

auto State::append_mined_block(const Block& b) -> StateUpdateWithAPIBlocks
{
    auto nextHeight { next_height() };
    if (nextHeight != b.height)
        throw Error(EMINEDDEPRECATED);
    auto prepared { chainstate.prepare_append(signedSnapshot, b.header) };
    if (!prepared.has_value())
        throw Error(prepared.error());
    if (chainlength() + 1 != b.height)
        throw Error(EBADHEIGHT);

    const auto nextStateId { db.next_state_id() };
    const auto nextHistoryId { db.next_history_id() };
    const auto nextAccountId { db.next_account_id() };

    // do db transaction for new block
    auto transaction = db.transaction();

    auto [blockId, inserted] { db.insert_protect(b) };
    if (!inserted) {
        spdlog::error("Mined block is already in database. This is a bug.");
        throw Error(EMINEDDEPRECATED);
    }

    chainserver::BlockApplier e { db, chainstate.headers(), chainstate.txids(), false };
    auto apiBlock { e.apply_block(b, prepared->hash, blockId) };
    db.set_consensus_work(chainstate.work_with_new_block());
    transaction.commit();
    dbcache.clear();

    std::unique_lock<std::mutex> ul(chainstateMutex);
    auto headerchainAppend = chainstate.append(Chainstate::AppendSingle {
        .wartUpdates { e.move_balance_updates() },
        .signedSnapshot { signedSnapshot },
        .prepared { prepared.value() },
        .newTxIds { e.move_new_txids() },
        .newHistoryOffset { nextHistoryId },
        .newAccountOffset { nextAccountId },
        .nextStateId = nextStateId });
    ul.unlock();

    dbCacheValidity += 1;
    return { .update {
                 .chainstateUpdate { state_update::Append {
                     headerchainAppend,
                     try_sign_chainstate() } },
                 .mempoolUpdates { chainstate.pop_mempool_updates() },
             },
        .appendedBlocks { std::move(apiBlock) } };
}

std::pair<mempool::Updates, TxHash> State::append_gentx(const WartTransferCreate& m)
{
    try {
        auto txhash { chainstate.create_tx(m) };
        auto log { chainstate.pop_mempool_updates() };
        spdlog::info("Added new transaction to mempool");
        return { std::move(log), std::move(txhash) };
    } catch (const Error& e) {
        spdlog::warn("Rejected new transaction: {}", e.strerror());
        throw;
    }
}

api::WartBalance State::api_get_address(AddressView address) const
{
    if (auto p = db.lookup_account(address); p) {
        return api::WartBalance {
            api::AddressWithId {
                address,
                p->accointId,
            },
            p->funds
        };
    } else {
        return api::WartBalance {
            {},
            Wart::zero()
        };
    }
}

api::WartBalance State::api_get_address(AccountId accountId) const
{
    if (auto p = db.lookup_address(accountId); p) {
        return api::WartBalance {
            api::AddressWithId {
                p->address,
                accountId },
            p->funds
        };
    } else {
        return {};
    }
}

std::optional<AssetInfo> State::db_lookup_token(const api::TokenIdOrHash& token) const
{
    return token.visit([&](const auto& token) { return db.lookup_asset(token); });
}

auto State::api_get_token_balance(const api::AccountIdOrAddress& account, const api::TokenIdOrHash& token) const -> api::WartBalance
{
    auto aid { account.map_alternative([&](const Address& a) { return db.lookup_account_id(a); }) };
    auto tokenInfo { token.visit([&](const auto& token) { return db.lookup_asset(token); }) };
    if (!aid || !tokenInfo)
        return {};
}

auto State::insert_txs(const TxVec& txs) -> std::pair<std::vector<Error>, mempool::Updates>
{
    std::vector<Error> res;
    res.reserve(txs.size());
    for (auto& tx : txs) {
        try {
            chainstate.insert_txs(tx);
            res.push_back(0);
        } catch (const Error& e) {
            res.push_back(e.code);
        }
    }
    return { res, chainstate.pop_mempool_updates() };
}

api::ChainHead State::api_get_head() const
{
    NonzeroHeight nextHeight { next_height() };
    PinFloor pf { nextHeight.pin_floor() };
    PinHeight ph { pf };
    return api::ChainHead {
        .signedSnapshot { signedSnapshot },
        .worksum { chainstate.headers().total_work() },
        .nextTarget { chainstate.headers().next_target() },
        .hash { chainstate.final_hash() },
        .height { chainlength() },
        .pinHash { chainstate.headers().hash_at(pf) },
        .pinHeight { PinHeight(pf) },
        .hashrate = chainstate.headers().hashrate_at(chainlength(), 100)
    };
}

auto State::api_get_mempool(size_t n) const -> api::MempoolEntries
{
    std::vector<Hash> hashes;
    auto nextHeight { next_height() };
    auto entries = chainstate.mempool().get_transactions(n, nextHeight, &hashes);
    assert(hashes.size() == entries.size());
    api::MempoolEntries out;
    for (size_t i = 0; i < hashes.size(); ++i) {
        out.entries.push_back(api::MempoolEntry {
            entries[i], hashes[i] });
    }
    return out;
}

auto State::api_get_history(Address a, int64_t beforeId) const -> std::optional<api::AccountHistory>
{
    auto p = db.lookup_account(a);
    if (!p)
        return {};
    auto& [accountId, balance] = *p;

    std::vector entries_desc = db.lookup_history_100_desc(accountId, beforeId);
    std::vector<api::Block> blocks_reversed;
    PinFloor pinFloor { 0 };
    auto firstHistoryId = HistoryId { 0 };
    auto nextHistoryOffset = HistoryId { 0 };
    chainserver::DBCache cache(db);

    auto prevHistoryId = HistoryId { 0 };
    for (auto iter = entries_desc.rbegin(); iter != entries_desc.rend(); ++iter) {
        auto& [historyId, txid, data] = *iter;
        if (firstHistoryId == HistoryId { 0 })
            firstHistoryId = historyId;
        assert(prevHistoryId < historyId);
        prevHistoryId = historyId;
        if (historyId >= nextHistoryOffset) {
            auto height { chainstate.history_height(historyId) };
            pinFloor = height.pin_floor();
            auto header = chainstate.headers()[height];
            bool b = height == chainlength();
            nextHistoryOffset = (b
                    ? HistoryId { std::numeric_limits<uint64_t>::max() }
                    : chainstate.historyOffset(height + 1));
            blocks_reversed.push_back(
                api::Block(header, height, 1 + (chainlength() - height)));
        }
        api::Block& b = blocks_reversed.back();
        b.push_history(txid, data, cache, pinFloor);
    }

    return api::AccountHistory {
        .balance = balance,
        .fromId = firstHistoryId,
        .blocks_reversed = blocks_reversed
    };
}

auto State::api_get_richlist(size_t N) const -> api::Richlist
{
    return db.lookup_richlist(N);
}

auto State::get_body_data(DescriptedBlockRange range) const -> std::vector<BodyContainer>
{
    assert(range.first() != 0);
    assert(range.last() >= range.first());
    std::vector<Hash> hashes(range.last() - range.first() + 1);
    std::vector<BodyContainer> res;
    if (range.descriptor == chainstate.descriptor()) {
        if (chainstate.length() < range.last())
            return {};
        for (Height h = range.first(); h < range.last() + 1; ++h) {
            hashes[h - range.first()] = chainstate.headers().hash_at(h);
        }
    } else {
        hashes = blockCache.get_hashes(range);
    }
    for (size_t i = 0; i < hashes.size(); ++i) {
        auto hash { hashes[i] };
        auto b { db.get_block_body(hash) };
        if (b) {
            res.push_back(std::move(b->second.body));
        } else {
            spdlog::error("BUG: no block with hash {} in db.", serialize_hex(hash));
            return {};
        }
    }
    return res;
}

auto State::get_mempool_tx(TransactionId txid) const -> std::optional<WartTransferMessage>
{
    return chainstate.mempool()[txid];
}

auto State::commit_fork(RollbackResult&& rr, AppendBlocksResult&& abr) -> StateUpdate
{
    assert(!signedSnapshot || signedSnapshot->compatible(stage));
    auto headers_ptr { blockCache.add_old_chain(chainstate, rr.deletionKey) };

    {
        std::unique_lock<std::mutex> lcons(chainstateMutex);
        chainstate.fork(chainserver::Chainstate::ForkData {
            .stage { stage },
            .rollbackResult { std::move(rr) },
            .appendResult { std::move(abr) },
        });
    }

    state_update::Fork forkMsg {
        chainstate.headers().get_fork(rr.shrink, chainstate.descriptor()),
        std::move(headers_ptr),
        try_sign_chainstate()
    };

    return StateUpdate {
        .chainstateUpdate { std::move(forkMsg) },
        .mempoolUpdates { chainstate.pop_mempool_updates() },
    };
}

auto State::commit_append(AppendBlocksResult&& abr) -> StateUpdate
{
    assert(!signedSnapshot || signedSnapshot->compatible(stage));
    {
        std::unique_lock<std::mutex> lcons(chainstateMutex);
        auto headerchainAppend { chainstate.append(Chainstate::AppendMulti {
            .patchedChain = stage,
            .appendResult { std::move(abr) },
        }) };
    }

    return {
        .chainstateUpdate {
            state_update::Append {
                headerchainAppend,
                try_sign_chainstate(),
            } },
        .mempoolUpdates { chainstate.pop_mempool_updates() },
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
}

MiningCache::CacheValidity State::mining_cache_validity()
{
    return { dbCacheValidity, chainstate.mempool().cache_validity(), now_timestamp() };
}

size_t State::api_db_size() const
{
    return db.byte_size();
}
}
