#include "server.hpp"
#include "api/types/all.hpp"
#include "block/header/header_impl.hpp"
#include "eventloop/eventloop.hpp"
#include "general/hex.hpp"
#include "global/globals.hpp"
#include "spdlog/spdlog.h"

bool ChainServer::is_busy()

{
    std::unique_lock<std::mutex> ul(mutex);
    return switching;
}

Batch ChainServer::get_headers(BatchSelector selector)
{
    return state.get_headers_concurrent(selector);
}

std::optional<HeaderView> ChainServer::get_descriptor_header(Descriptor descriptor, Height height)
{
    return state.get_header_concurrent(descriptor, height);
}

ConsensusSlave ChainServer::get_chainstate()
{
    return state.get_chainstate_concurrent();
}

ChainServer::ChainServer(ChainDB& db, BatchRegistry& br, std::optional<SnapshotSigner> snapshotSigner, Token)
    : db(db)
    , batchRegistry(br)
    , state(db, br, snapshotSigner)
{
    worker = std::thread(&ChainServer::workerfun, this);
}

ChainServer::~ChainServer()
{
    close();
}

void ChainServer::api_mining_append(Block&& block, ResultCb callback)
{
    defer_maybe_busy(MiningAppend { std::move(block), std::move(callback) });
}

void ChainServer::async_set_synced(bool synced)
{
    spdlog::debug("Set synced {}", synced);
    defer(SetSynced { synced });
}

void ChainServer::async_put_mempool(std::vector<TransferTxExchangeMessage> txs)
{
    defer(PutMempoolBatch { std::move(txs) });
}

void ChainServer::api_put_mempool(PaymentCreateMessage m,
    MempoolInsertCb callback)
{
    defer_maybe_busy(PutMempool { std::move(m), std::move(callback) });
}

void ChainServer::api_get_balance(const API::AccountIdOrAddress& a, BalanceCb callback)
{
    defer_maybe_busy(GetBalance { a, std::move(callback) });
}

void ChainServer::api_get_grid(GridCb callback)
{
    defer_maybe_busy(GetGrid { std::move(callback) });
}

void ChainServer::api_get_mempool(MempoolCb callback)
{
    defer_maybe_busy(GetMempool { std::move(callback) });
}

void ChainServer::api_lookup_tx(const HashView hash,
    TxCb callback)
{
    defer_maybe_busy(LookupTxHash { hash, std::move(callback) });
}
void ChainServer::api_lookup_latest_txs(LatestTxsCb callback)
{
    defer_maybe_busy(LookupLatestTxs { std::move(callback) });
}

void ChainServer::async_get_head(HeadCb callback)
{
    defer_maybe_busy(GetHead { std::move(callback) });
}

void ChainServer::api_get_history(const Address& address, uint64_t beforeId,
    HistoryCb callback)
{
    defer_maybe_busy(GetHistory { address, beforeId, std::move(callback) });
}

void ChainServer::api_get_richlist(RichlistCb callback)
{
    defer_maybe_busy(GetRichlist { std::move(callback) });
}
void ChainServer::api_get_mining(const Address& address, MiningCb callback)
{
    defer_maybe_busy(GetMining { address, std::move(callback) });
}

auto ChainServer::api_subscribe_mining(Address address, mining_subscription::callback_t callback) -> mining_subscription::MiningSubscription
{
    auto req { SubscribeMining::make(address, callback) };
    auto id { req.id };
    defer(std::move(req));
    return { shared_from_this(), id };
}

void ChainServer::api_unsubscribe_mining(mining_subscription::SubscriptionId id)
{
    defer(UnsubscribeMining { id });
}

void ChainServer::api_get_txcache(TxcacheCb callback)
{
    defer_maybe_busy(GetTxcache { std::move(callback) });
}

void ChainServer::api_get_header(API::HeightOrHash hoh, HeaderCb callback)
{
    defer_maybe_busy(GetHeader { hoh, std::move(callback) });
}
void ChainServer::api_get_hash(Height height, HashCb callback)
{
    defer_maybe_busy(GetHash { height, std::move(callback) });
}

void ChainServer::api_get_block(API::HeightOrHash hoh, BlockCb callback)
{
    defer_maybe_busy(GetBlock { hoh, std::move(callback) });
}

void ChainServer::async_get_blocks(DescriptedBlockRange range, getBlocksCb&& callback)
{
    defer(GetBlocks { range, std::move(callback) });
}

void ChainServer::async_stage_request(stage_operation::Operation r)
{
    std::visit([&](auto req) {
        defer(std::move(req));
    },
        std::move(r));
}

void ChainServer::async_set_signed_checkpoint(SignedSnapshot ss)
{
    defer(SetSignedPin { ss });
}

void ChainServer::close()
{
    std::unique_lock<std::mutex> ul(mutex);
    closing = true;
    haswork = true;
    cv.notify_one();
}

void ChainServer::workerfun()
{
    // initialization
    while (true) {
        {
            std::unique_lock<std::mutex> ul(mutex);
            while (!haswork) {
                cv.wait(ul);
            }
        }
        haswork = false;
        if (closing)
            break;
        state.garbage_collect();

        { // work
            decltype(events) tmpq;
            {
                std::unique_lock<std::mutex> ul(mutex);
                std::swap(tmpq, events);
            }
            while (!tmpq.empty()) {
                std::visit([&](auto&& e) {
                    handle_event(std::move(e));
                },
                    tmpq.front());
                tmpq.pop();
            }
        }
    }
}

void ChainServer::dispatch_mining_subscriptions()
{
    miningSubscriptions.dispatch([&](const Address& a) {
        return state.mining_task(a);
    });
}

TxHash ChainServer::append_gentx(const PaymentCreateMessage& m)
{
    auto [log, txhash] = state.append_gentx(m);
    global().pel->async_mempool_update(std::move(log));
    return txhash;
}

void ChainServer::handle_event(MiningAppend&& e)
{
    try {
        auto res = state.append_mined_block(e.block);
        global().pel->async_state_update(std::move(res));
        spdlog::info("Accepted new block #{}", state.chainlength().value());
        e.callback({});
        dispatch_mining_subscriptions();
    } catch (Error err) {
        spdlog::info("Rejected new block #{}: {}", (state.chainlength() + 1).value(),
            err.strerror());
        e.callback(tl::make_unexpected(err.e));
    }
}

void ChainServer::handle_event(GetGrid&& e)
{
    e.callback(state.get_headers().grid());
}

void ChainServer::handle_event(GetBalance&& e)
{
    auto result = e.account.visit([&](const auto& t) { return state.api_get_address(t); });
    e.callback(result);
}

void ChainServer::handle_event(GetMempool&& e)
{
    e.callback(state.api_get_mempool(2000));
}

void ChainServer::handle_event(LookupTxids&& e)
{
    std::vector<std::optional<TransferTxExchangeMessage>> out;
    std::transform(e.txids.begin(), e.txids.end(), std::back_inserter(out),
        [&](auto txid) { return state.get_mempool_tx(txid); });
    e.callback(out);
}

template <typename T>
tl::expected<T, int32_t> noval_to_err(std::optional<T>&& v)
{
    if (v)
        return *v;
    return tl::make_unexpected(ENOTFOUND);
}

void ChainServer::handle_event(LookupTxHash&& e)
{
    e.callback(noval_to_err(state.api_get_tx(e.hash)));
}

void ChainServer::handle_event(LookupLatestTxs&& e)
{
    e.callback(state.api_get_latest_txs());
};

void ChainServer::handle_event(SetSynced&& e)
{
    state.set_sync_state(e.synced);
}

void ChainServer::handle_event(GetHistory&& e)
{
    auto history { state.api_get_history(e.address, e.beforeId) };
    e.callback(noval_to_err(std::move(history)));
}

void ChainServer::handle_event(GetRichlist&& e)
{
    auto richlist { state.api_get_richlist(100) };
    e.callback(std::move(richlist));
}

void ChainServer::handle_event(GetHead&& e)
{
    e.callback(state.api_get_head());
}

void ChainServer::handle_event(GetHeader&& e)
{
    e.callback(noval_to_err(state.api_get_header(e.heightOrHash)));
}

void ChainServer::handle_event(GetHash&& e)
{
    e.callback(noval_to_err(state.get_hash(e.height)));
}

void ChainServer::handle_event(GetBlock&& e)
{
    e.callback(noval_to_err(state.api_get_block(e.heightOrHash)));
}

void ChainServer::handle_event(GetMining&& e)
{
    auto mt = state.mining_task(e.address);
    e.callback(mt);
}

void ChainServer::handle_event(SubscribeMining&& e)
{
    e.callback(state.mining_task(e.address));
    miningSubscriptions.subscribe(std::move(e));
}

void ChainServer::handle_event(UnsubscribeMining&& e)
{
    miningSubscriptions.unsubscribe(e.id);
}

void ChainServer::handle_event(GetTxcache&& e)
{
    e.callback(state.api_tx_cache());
}
//
void ChainServer::handle_event(GetBlocks&& e)
{
    e.callback(state.get_blocks(e.range));
}

void ChainServer::handle_event(stage_operation::StageSetOperation&& r)
{
    global().pel->async_stage_action(state.set_stage(std::move(r.headers)));
}

void ChainServer::handle_event(stage_operation::StageAddOperation&& r)
{
    auto [stageAddResult, delta] { state.add_stage(r.blocks, r.headers) };
    if (delta) {
        global().pel->async_state_update(std::move(*delta));
        dispatch_mining_subscriptions();
    }
    global().pel->async_stage_action(std::move(stageAddResult));
}

void ChainServer::handle_event(PutMempool&& e)
{
    try {
        auto txhash { append_gentx(std::move(e.m)) };
        e.callback(txhash);
    } catch (Error err) {
        e.callback(tl::make_unexpected(err.e));
    }
}

void ChainServer::handle_event(PutMempoolBatch&& mb)
{
    auto [_, log] { state.insert_txs(mb.txs) };
    // LATER: introduce some logic to ban
    // peers who sent such bad transactions
    global().pel->async_mempool_update(std::move(log));
}

void ChainServer::handle_event(SetSignedPin&& e)
{
    auto res { state.apply_signed_snapshot(std::move(e.ss)) };
    if (res) {
        global().pel->defer(std::move(*res));
    }
}
