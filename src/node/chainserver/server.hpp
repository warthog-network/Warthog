#pragma once
#include "api/callbacks.hpp"
#include "api/events/subscription_fwd.hpp"
#include "api/types/accountid_or_address.hpp"
#include "api/types/height_or_hash.hpp"
#include "chainserver/mining_subscription.hpp"
#include "chainserver/subscription_state.hpp"
#include "communication/create_payment.hpp"
#include "communication/stage_operation/request.hpp"
#include "general/logging.hpp"
#include "state/state.hpp"
#include <condition_variable>
#include <queue>
#include <thread>

class ChainServer : public std::enable_shared_from_this<ChainServer> {
    using getBlocksCb = std::function<void(std::vector<BodyContainer>&&)>;

public:
    // can be called concurrently
    Batch get_headers(BatchSelector selector);
    std::optional<HeaderView> get_descriptor_header(Descriptor descriptor, Height height);
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
    struct GetWartBalance {
        api::AccountIdOrAddress account;
        BalanceCb callback;
    };
    struct GetTokenBalance {
        api::AccountIdOrAddress account;
        api::TokenIdOrHash token;
        BalanceCb callback;
    };
    struct GetMempool {
        MempoolCb callback;
    };
    struct LookupTxids {
        Height maxHeight;
        std::vector<TransactionId> txids;
        MempoolTxsCb callback;
    };
    struct LookupTxHash {
        const Hash hash;
        TxCb callback;
    };
    struct LookupLatestTxs {
        LatestTxsCb callback;
    };
    struct SetSynced {
        bool synced;
    };
    struct GetHistory {
        const Address& address;
        uint64_t beforeId;
        HistoryCb callback;
    };
    struct GetRichlist {
        RichlistCb callback;
    };
    struct GetHead {
        ChainHeadCb callback;
    };
    struct GetHeader {
        api::HeightOrHash heightOrHash;
        HeaderCb callback;
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
    struct PutMempoolBatch {
        std::vector<WartTransferMessage> txs;
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
    using Event = std::variant<
        MiningAppend,
        PutMempool,
        GetGrid,
        GetWartBalance,
        GetTokenBalance,
        GetMempool,
        LookupTxids,
        LookupTxHash,
        LookupLatestTxs,
        SetSynced,
        GetHistory,
        GetRichlist,
        GetHead,
        GetHeader,
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
            e.callback(tl::make_unexpected(ESWITCHING));
        else {
            haswork = true;
            events.emplace(std::forward<T>(e));
            cv.notify_one();
        }
    }

    struct Token { };

public:
    ChainServer(ChainDB& b, BatchRegistry&, std::optional<SnapshotSigner> snapshotSigner, Token);
    static auto make_chain_server(ChainDB& b, BatchRegistry& br, std::optional<SnapshotSigner> snapshotSigner)
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

    void async_set_synced(bool synced);

    void async_put_mempool(std::vector<WartTransferMessage> txs);
    void async_get_head(ChainHeadCb callback);

    // API methods
    void api_mining_append(BlockWorker&&, ResultCb);
    // void api_put_mempool(PaymentCreateMessage, ResultCb cb);
    void api_put_mempool(WartTransferCreate, MempoolInsertCb cb);
    void api_get_wart_balance(const api::AccountIdOrAddress& a, BalanceCb callback);
    void api_get_token_balance(const api::AccountIdOrAddress& a, const api::TokenIdOrHash&, BalanceCb callback);
    void api_get_grid(GridCb);
    void api_get_mempool(MempoolCb callback);
    void api_lookup_tx(Hash hash, TxCb callback);
    void api_lookup_latest_txs(LatestTxsCb callback);
    void api_get_history(const Address& address, uint64_t beforeId, HistoryCb callback);
    void api_get_richlist(RichlistCb callback);
    void api_get_header(api::HeightOrHash, HeaderCb callback);
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
    void handle_event(MiningAppend&&);
    void handle_event(PutMempool&&);
    void handle_event(GetGrid&&);
    void handle_event(GetWartBalance&&);
    void handle_event(GetTokenBalance&&);
    void handle_event(GetMempool&&);
    void handle_event(LookupTxids&&);
    void handle_event(LookupTxHash&&);
    void handle_event(LookupLatestTxs&&);
    void handle_event(SetSynced&& e);
    void handle_event(GetHistory&&);
    void handle_event(GetRichlist&&);
    void handle_event(GetHead&&);
    void handle_event(GetHeader&&);
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
    std::optional<logging::TimingSession> timing;

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
