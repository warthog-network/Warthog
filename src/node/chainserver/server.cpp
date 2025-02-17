#include "server.hpp"
#include "api/events/emit.hpp"
#include "api/events/subscription.hpp"
#include "api/types/all.hpp"
#include "block/header/header_impl.hpp"
#include "eventloop/eventloop.hpp"
#include "global/globals.hpp"

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
    emit_chain_state_event();
}

ChainServer::~ChainServer()
{
    wait_for_shutdown();
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

void ChainServer::api_get_balance(const api::AccountIdOrAddress& a, BalanceCb callback)
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

void ChainServer::async_get_head(ChainHeadCb callback)
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
void ChainServer::api_get_mining(const Address& address, ChainMiningCb callback)
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

void ChainServer::api_get_db_size(DBSizeCB callback)
{
    GetDBSize db{ std::move(callback) };
    defer_maybe_busy(db);
}


void ChainServer::api_get_header(api::HeightOrHash hoh, HeaderCb callback)
{
    defer_maybe_busy(GetHeader { hoh, std::move(callback) });
}
void ChainServer::api_get_hash(Height height, HashCb callback)
{
    defer_maybe_busy(GetHash { height, std::move(callback) });
}

void ChainServer::api_get_block(api::HeightOrHash hoh, BlockCb callback)
{
    defer_maybe_busy(GetBlock { hoh, std::move(callback) });
}

void ChainServer::subscribe_account_event(SubscriptionRequest r, Address a)
{
    defer(SubscribeAccount { std::move(r), std::move(a) });
}
void ChainServer::subscribe_chain_event(SubscriptionRequest r)
{
    defer(SubscribeChain { std::move(r) });
}
void ChainServer::subscribe_minerdist_event(SubscriptionRequest r)
{
    defer(SubscribeMinerdist { std::move(r) });
}

void ChainServer::destroy_subscriptions(subscription_data_ptr p)
{
    defer(DestroySubscriptions { p });
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

void ChainServer::shutdown()
{
    std::unique_lock<std::mutex> ul(mutex);
    closing = true;
    haswork = true;
    cv.notify_one();
}

void ChainServer::wait_for_shutdown()
{
    if (worker.joinable())
        worker.join();
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
            timing = timing_log().session();
            while (!tmpq.empty()) {
                std::visit([&](auto&& e) {
                    handle_event(std::move(e));
                },
                    tmpq.front());
                tmpq.pop();
            }
            timing.reset();
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
    global().core->async_mempool_update(std::move(log));
    return txhash;
}

void ChainServer::on_chain_changed(StateUpdateWithAPIBlocks&& su)
{
    emit_chain_state_event();

    subscription_state::NewBlockInfo nbi {
        su.update.chainstateUpdate.rollback(),
        su.appendedBlocks
    };
    minerdistSubscriptions.on_chain_changed(state, nbi);
    addressSubscriptions.on_chain_changed(state, nbi);
    chainSubscriptions.on_chain_changed(state, nbi);

    // rollback api actions
    if (auto s { su.update.chainstateUpdate.rollback() })
        api::event::emit_rollback(s->length);

    // incremental block api actions
    for (auto& b : su.appendedBlocks)
        api::event::emit_block_append(std::move(b));

    global().core->async_state_update(std::move(su.update));
    dispatch_mining_subscriptions();
}

void ChainServer::handle_event(MiningAppend&& e)
{
    auto t { timing->time("MiningAppend") };
    try {
        auto res = state.append_mined_block(e.block);
        on_chain_changed(std::move(res));
        spdlog::info("Accepted new block #{}", state.chainlength().value());
        e.callback({});
        dispatch_mining_subscriptions();
    } catch (Error err) {
        spdlog::info("Rejected new block #{}: {}", (state.chainlength() + 1).value(),
            err.strerror());
        e.callback(tl::make_unexpected(err.code));
    }
}

void ChainServer::handle_event(GetGrid&& e)
{
    auto t{timing->time("GetGrid")};
    e.callback(state.get_headers().grid());
}

void ChainServer::handle_event(GetBalance&& e)
{
    auto t{timing->time("GetBalance")};
    auto result = e.account.visit([&](const auto& t) { return state.api_get_address(t); });
    e.callback(result);
}

void ChainServer::handle_event(GetMempool&& e)
{
    auto t{timing->time("GetMempool")};
    e.callback(state.api_get_mempool(2000));
}

void ChainServer::handle_event(LookupTxids&& e)
{
    auto t{timing->time("LookupTxIds")};
    std::vector<std::optional<TransferTxExchangeMessage>> out;
    std::transform(e.txids.begin(), e.txids.end(), std::back_inserter(out),
        [&](auto txid) { return state.get_mempool_tx(txid); });
    e.callback(out);
}

void ChainServer::emit_chain_state_event()
{
    api::event::emit_chain_state({ .length { state.chainlength() },
        .target { state.get_headers().next_target() },
        .totalWork { state.get_headers().total_work() } });
}

template <typename T>
tl::expected<T, Error> noval_to_err(std::optional<T>&& v)
{
    if (v)
        return *v;
    return tl::make_unexpected(Error(ENOTFOUND));
}

void ChainServer::handle_event(LookupTxHash&& e)
{
    auto t{timing->time("LookupTxHash")};
    e.callback(noval_to_err(state.api_get_tx(e.hash)));
}

void ChainServer::handle_event(LookupLatestTxs&& e)
{
    auto t{timing->time("LookupLatestTxs")};
    e.callback(state.api_get_latest_txs());
};

void ChainServer::handle_event(SetSynced&& e)
{
    auto t{timing->time("SetSynced")};
    state.set_sync_state(e.synced);
}

void ChainServer::handle_event(GetHistory&& e)
{
    auto t{timing->time("GetHistory")};
    auto history { state.api_get_history(e.address, e.beforeId) };
    e.callback(noval_to_err(std::move(history)));
}

void ChainServer::handle_event(GetRichlist&& e)
{
    auto t{timing->time("GetRichlist")};
    auto richlist { state.api_get_richlist(100) };
    e.callback(std::move(richlist));
}

void ChainServer::handle_event(GetHead&& e)
{
    auto t{timing->time("GetHead")};
    e.callback(state.api_get_head());
}

void ChainServer::handle_event(GetHeader&& e)
{
    auto t{timing->time("GetHeader")};
    e.callback(noval_to_err(state.api_get_header(e.heightOrHash)));
}

void ChainServer::handle_event(GetHash&& e)
{
    auto t{timing->time("GetHash")};
    e.callback(noval_to_err(state.get_hash(e.height)));
}

void ChainServer::handle_event(GetBlock&& e)
{
    auto t{timing->time("GetBlock")};
    e.callback(noval_to_err(state.api_get_block(e.heightOrHash)));
}

void ChainServer::handle_event(GetMining&& e)
{
    auto t{timing->time("GetMining")};
    auto mt = state.mining_task(e.address);
    e.callback(mt);
}

void ChainServer::handle_event(SubscribeMining&& e)
{
    auto t{timing->time("SubscribeMining")};
    e.callback(state.mining_task(e.address));
    miningSubscriptions.subscribe(std::move(e));
}

void ChainServer::handle_event(UnsubscribeMining&& e)
{
    auto t{timing->time("UnsubscribeMining")};
    miningSubscriptions.unsubscribe(e.id);
}

void ChainServer::handle_event(GetTxcache&& e)
{
    auto t{timing->time("GetTxcache")};
    e.callback(state.api_tx_cache());
}
void ChainServer::handle_event(GetDBSize&& e)
{
    e.callback(api::DBSize{state.api_db_size()});
}
//
void ChainServer::handle_event(GetBlocks&& e)
{
    auto t{timing->time("GetBlocks")};
    e.callback(state.get_blocks(e.range));
}

void ChainServer::handle_event(stage_operation::StageSetOperation&& r)
{
    auto t{timing->time("StageSet")};
    global().core->async_stage_action(state.set_stage(std::move(r.headers)));
}

void ChainServer::handle_event(stage_operation::StageAddOperation&& r)
{
    auto t{timing->time("StageAdd")};
    auto res { state.add_stage(r.blocks, r.headers) };
    if (res.update)
        on_chain_changed(std::move(*res.update));
    if (res.rogueHeaderData) 
        global().core->async_push_rogue(*res.rogueHeaderData);
    global().core->async_stage_action(res.status);
}

void ChainServer::handle_event(PutMempool&& e)
{
    auto t{timing->time("PutMempool")};
    try {
        auto txhash { append_gentx(std::move(e.m)) };
        e.callback(txhash);
    } catch (Error err) {
        e.callback(tl::make_unexpected(err.code));
    }
}

void ChainServer::handle_event(PutMempoolBatch&& mb)
{
    auto t{timing->time("PutMempoolBatch")};
    auto [_, log] { state.insert_txs(mb.txs) };
    // LATER: introduce some logic to ban
    // peers who sent such bad transactions
    global().core->async_mempool_update(std::move(log));
}

void ChainServer::handle_event(SetSignedPin&& e)
{
    auto t{timing->time("SetSignedPin")};
    auto res { state.apply_signed_snapshot(std::move(e.ss)) };
    if (res)
        on_chain_changed(std::move(*res));
}
void ChainServer::handle_event(SubscribeAccount&& s)
{
    addressSubscriptions.handle_subscription(std::move(s.req), state, s.addr);
}

void ChainServer::handle_event(SubscribeChain&& s)
{
    chainSubscriptions.handle_subscription(std::move(s), state);
}
void ChainServer::handle_event(SubscribeMinerdist&& s)
{
    minerdistSubscriptions.handle_subscription(std::move(s), state);
}

void ChainServer::handle_event(DestroySubscriptions&& s)
{
    addressSubscriptions.erase_all(s.p);
    chainSubscriptions.erase_all(s.p);
}

