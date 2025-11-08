#pragma once
#include "api/callbacks.hpp"
#include "api/events/subscription_fwd.hpp"
#include "api/types/accountid_or_address.hpp"
#include "api/types/height_or_hash.hpp"
#include "api_types.hpp"
#include "chainserver/mining_subscription.hpp"
#include "chainserver/subscription_state.hpp"
#include "communication/create_transaction.hpp"
#include "communication/stage_operation/request.hpp"
#include "general/logging.hpp"
#include "state/state.hpp"
#include <condition_variable>
#include <queue>
#include <thread>

#define LIST_API_TYPES(XX)                                    \
    XX(MiningAppend, void, Block, block, std::string, worker) \
    XX(PutMempool, TxHash, WartTransferCreate, message)       \
    XX(LatestTxs, api::TransactionsByBlocks)                  \
    XX(LookupTxHash, api::Transaction, TxHash, hash)          \
    XX(GetTransactionMinfee, api::TransactionMinfee)          \
    XX(GetRichlist, api::Richlist, api::TokenIdOrSpec, token)

namespace chainserver {
DEFINE_TYPE_COLLECTION(APITypes, LIST_API_TYPES);
}

#undef LIST_API_TYPES

class ChainServer : public std::enable_shared_from_this<ChainServer>, public enable_api_methods<ChainServer, chainserver::APITypes> {
    using getBlocksCb = std::function<void(std::vector<BodyData>&&)>;
    friend enable_api_methods;

public:
    // can be called concurrently
    Batch get_headers(BatchSelector selector);
    wrt::optional<HeaderView> get_descriptor_header(Descriptor descriptor, Height height);
    ConsensusSlave get_chainstate();

    void shutdown();
    void wait_for_shutdown();

    struct MiningAppend {
        Block block;
        std::string worker;
        ResultCb callback;
    };
    struct PutMempool {
        WartTransferCreate m;
        MempoolInsertCb callback;
    };
    struct GetGrid {
        GridCb callback;
    };
    struct GetTokenBalance {
        api::AccountIdOrAddress account;
        api::TokenIdOrSpec token;
        TokenBalanceCb callback;
    };
    struct GetMempool {
        MempoolCb callback;
    };
    struct LookupTxids {
        Height maxHeight;
        std::vector<TransactionId> txids;
        MempoolTxsCb callback;
    };
    struct SetSynced {
        bool synced;
    };
    struct GetHistory {
        const Address& address;
        uint64_t beforeId;
        HistoryCb callback;
    };
    struct GetHead {
        ChainHeadCb callback;
    };
    struct GetHeader {
        api::HeightOrHash heightOrHash;
        HeaderCb callback;
    };
    struct GetBlockBinary {
        api::HeightOrHash heightOrHash;
        BlockBinaryCb callback;
    };
    struct GetHash {
        Height height;
        HashCb callback;
    };
    struct GetBlock {
        api::HeightOrHash heightOrHash;
        BlockCb callback;
    };
    struct GetMining {
        Address address;
        ChainMiningCb callback;
    };
    using SubscribeMining = mining_subscription::SubscriptionRequest;
    struct UnsubscribeMining {
        mining_subscription::SubscriptionId id;
    };
    struct GetTxcache {
        TxcacheCb callback;
    };
    struct GetDBSize {
        DBSizeCB callback;
    };
    struct GetBlocks {
        DescriptedBlockRange range;
        getBlocksCb callback;
    };
    struct MempoolConstraintUpdate {
        MempoolConstraintCb callback;
    };
    struct PutMempoolBatch {
        std::vector<TransactionMessage> txs;
    };
    struct SetSignedPin {
        SignedSnapshot ss;
    };

    // subscription related
    struct SubscribeAccount {
        SubscriptionRequest req;
        Address addr;
    };

    struct SubscribeChain : public SubscriptionRequest {
    };
    struct SubscribeMinerdist : public SubscriptionRequest {
    };

    struct DestroySubscriptions {
        subscription_data_ptr p;
    };

    // EVENTS
    using Event = events_t<
        MiningAppend,
        PutMempool,
        GetGrid,
        GetTokenBalance,
        GetMempool,
        LookupTxids,
        SetSynced,
        GetHistory,
        GetHead,
        GetHeader,
        GetBlockBinary,
        GetHash,
        GetBlock,
        GetMining,
        SubscribeMining,
        UnsubscribeMining,
        GetTxcache,
        GetDBSize,
        GetBlocks,
        stage_operation::StageAddOperation,
        stage_operation::StageSetOperation,
        MempoolConstraintUpdate,
        PutMempoolBatch,
        SetSignedPin,
        SubscribeAccount,
        SubscribeChain,
        SubscribeMinerdist,
        DestroySubscriptions>;

private:
    template <typename T>
    void defer(T&& e)
    {
        std::unique_lock l(mutex);
        haswork = true;
        events.emplace(std::forward<T>(e));
        cv.notify_one();
    }

    template <typename T>
    void defer_maybe_busy(T&& e)
    {
        std::unique_lock l(mutex);
        if (switching)
            e.callback(Error(ESWITCHING));
        else {
            haswork = true;
            events.emplace(std::forward<T>(e));
            cv.notify_one();
        }
    }

    struct Token { };

public:
    ChainServer(ChainDB& b, BatchRegistry&, wrt::optional<SnapshotSigner> snapshotSigner, Token);
    static auto make_chain_server(ChainDB& b, BatchRegistry& br, wrt::optional<SnapshotSigner> snapshotSigner)
    {
        return std::make_shared<ChainServer>(b, br, snapshotSigner, Token {});
    }
    void start()
    {
        assert(!worker.joinable());
        worker = std::thread(&ChainServer::workerfun, this);
    }
    ~ChainServer();

    bool is_busy();

    template <typename T>
    requires(supports<T>)
    void api_call(T&& req, T::Callback cb)
    {
        defer(typename std::remove_cvref_t<T>::Object(std::forward<T>(req), std::move(cb)));
    }

    void async_set_synced(bool synced);
    void async_notify_mempool_constraint_update(MempoolConstraintCb);

    void async_put_mempool(std::vector<TransactionMessage> txs);
    void async_get_head(ChainHeadCb callback);

    // API methods
    void api_mining_append(BlockWorker&&, ResultCb);
    // void api_put_mempool(PaymentCreateMessage, ResultCb cb);
    void api_fake_mine(ResultCb cb);
    void api_put_mempool(WartTransferCreate, MempoolInsertCb cb);
    void api_get_token_balance(const api::AccountIdOrAddress& a, const api::TokenIdOrSpec&, TokenBalanceCb callback);
    void api_get_grid(GridCb);
    void api_get_mempool(MempoolCb callback);
    void api_lookup_latest_txs(LatestTxsCb callback);
    void api_get_history(const Address& address, uint64_t beforeId, HistoryCb callback);
    // void api_get_richlist(api::TokenIdOrSpec tokenId, RichlistCb callback);
    void api_get_header(api::HeightOrHash, HeaderCb callback);
    void api_get_block_binary(api::HeightOrHash hoh, BlockBinaryCb callback);
    void api_get_hash(Height height, HashCb callback);
    void api_get_block(api::HeightOrHash, BlockCb callback);
    void api_get_mining(const Address& a, ChainMiningCb callback);
    [[nodiscard]] mining_subscription::MiningSubscription api_subscribe_mining(Address address, mining_subscription::callback_t callback);
    void api_unsubscribe_mining(mining_subscription::SubscriptionId);
    void api_get_txcache(TxcacheCb callback);
    void api_get_db_size(DBSizeCB callback);

    void subscribe_account_event(SubscriptionRequest, Address a);
    void subscribe_chain_event(SubscriptionRequest);
    void subscribe_minerdist_event(SubscriptionRequest);
    void destroy_subscriptions(subscription_data_ptr);

    void async_set_signed_checkpoint(SignedSnapshot);
    void async_get_blocks(DescriptedBlockRange, getBlocksCb&&);

    void async_stage_request(stage_operation::Operation);

private:
    ChainError apply_stage(ChainDBTransaction&& t);
    void workerfun();
    void dispatch_mining_subscriptions();

    TxHash append_gentx(const WartTransferCreate&);

private:
    auto handle_api(chainserver::PutMempool&&) -> TxHash;
    auto handle_api(chainserver::MiningAppend&&) -> void;
    auto handle_api(chainserver::LatestTxs&&) -> api::TransactionsByBlocks;
    auto handle_api(chainserver::LookupTxHash&&) -> api::Transaction;
    auto handle_api(chainserver::GetTransactionMinfee&&) { return state.api_get_transaction_minfee(); }
    auto handle_api(chainserver::GetRichlist&& e) { return state.api_get_richlist(e.token(), 100); }

    void handle_event(MiningAppend&&);
    void handle_event(PutMempool&&);
    void handle_event(GetGrid&&);
    void handle_event(GetTokenBalance&&);
    void handle_event(GetMempool&&);
    void handle_event(LookupTxids&&);
    void handle_event(SetSynced&& e);
    void handle_event(GetHistory&&);
    void handle_event(GetHead&&);
    void handle_event(GetHeader&&);
    void handle_event(GetBlockBinary&&);
    void handle_event(GetHash&&);
    void handle_event(GetBlock&&);
    void handle_event(GetMining&&);
    void handle_event(SubscribeMining&&);
    void handle_event(UnsubscribeMining&&);
    void handle_event(GetTxcache&&);
    void handle_event(GetDBSize&&);
    void handle_event(GetBlocks&&);
    void handle_event(stage_operation::StageSetOperation&&);
    void handle_event(stage_operation::StageAddOperation&&);
    void handle_event(MempoolConstraintUpdate&&);
    void handle_event(PutMempoolBatch&&);
    void handle_event(SetSignedPin&&);
    void handle_event(SubscribeAccount&&);
    void handle_event(SubscribeChain&&);
    void handle_event(SubscribeMinerdist&&);
    void handle_event(DestroySubscriptions&&);

    using StateUpdateWithAPIBlocks = chainserver::state_update::StateUpdateWithAPIBlocks;
    void on_chain_changed(StateUpdateWithAPIBlocks&&);

    void emit_chain_state_event();

    std::condition_variable cv;
    ChainDB& db;
    BatchRegistry& batchRegistry;

    // state variables
    chainserver::State state;

    // mutex protected variables
    std::mutex mutex;
    std::queue<Event> events;
    MiningSubscriptions miningSubscriptions;
    AddressSubscriptionState addressSubscriptions;
    ChainSubscriptionState chainSubscriptions;
    MinerdistSubscriptionState minerdistSubscriptions;

    //
    bool haswork = false;
    bool closing = false;
    bool switching = false; // doing chain switch?
    std::thread worker;
};
;
