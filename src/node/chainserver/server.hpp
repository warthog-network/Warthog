#pragma once
#include "api/callbacks.hpp"
#include "communication/stage_operation/request.hpp"
#include "state/state.hpp"
#include <condition_variable>
#include <queue>
#include <thread>

class ChainServer {
    using getBlocksCb = std::function<void(std::vector<BodyContainer>&&)>;

private:
    void garbage_collect();
    void garbage_collect_chains();

    struct BlocksReq {
        Descriptor descriptor;
        Height lowerHeight;
        Height upperHeight;
        getBlocksCb callback;
    };

public:
    // can be called concurrently
    Batch get_headers(BatchSelector selector);
    std::optional<HeaderView> get_descriptor_header(Descriptor descriptor, Height height);
    ConsensusSlave get_chainstate();

    void shutdown_join()
    {
        close();
        if (worker.joinable()) {
            worker.join();
        }
    }

    struct MiningAppend {
        Block block;
        ResultCb callback;
    };
    struct PutMempool {
        std::vector<uint8_t> data;
        ResultCb callback;
    };
    struct GetGrid {
        GridCb callback;
    };
    struct GetBalance {
        Address address;
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
        HeadCb callback;
    };
    struct GetHeader {
        Height height;
        HeaderCb callback;
    };
    struct GetHash {
        Height height;
        HashCb callback;
    };
    struct GetBlock {
        Height height;
        BlockCb callback;
    };
    struct GetMining {
        Address address;
        MiningCb callback;
    };
    struct GetTxcache {
        TxcacheCb callback;
    };
    struct GetBlocks {
        DescriptedBlockRange range;
        getBlocksCb callback;
    };
    struct PutMempoolBatch {
        std::vector<TransferTxExchangeMessage> txs;
    };
    struct SetSignedPin {
        SignedSnapshot ss;
    };

    // EVENTS
    using Event = std::variant<
        MiningAppend,
        PutMempool,
        GetGrid,
        GetBalance,
        GetMempool,
        LookupTxids,
        LookupTxHash,
        SetSynced,
        GetHistory,
        GetRichlist,
        GetHead,
        GetHeader,
        GetHash,
        GetBlock,
        GetMining,
        GetTxcache,
        GetBlocks,
        stage_operation::StageAddOperation,
        stage_operation::StageSetOperation,
        PutMempoolBatch,
        SetSignedPin>;

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

public:
    ChainServer(ChainDB& b, BatchRegistry&, std::optional<SnapshotSigner> snapshotSigner);
    ~ChainServer();

    bool is_busy();

    void async_set_synced(bool synced);

    void async_put_mempool(std::vector<TransferTxExchangeMessage> txs);
    void async_get_head(HeadCb callback);

    // API methods
    void api_mining_append(Block&&, ResultCb);
    void api_put_mempool(std::vector<uint8_t> data, ResultCb cb);
    void api_get_balance(const Address& a, BalanceCb callback);
    void api_get_grid(GridCb);
    void api_get_mempool(MempoolCb callback);
    void api_lookup_tx(const HashView hash, TxCb callback);
    void api_get_history(const Address& address, uint64_t beforeId, HistoryCb callback);
    void api_get_richlist(RichlistCb callback);
    void api_get_header(Height height, HeaderCb callback);
    void api_get_hash(Height height, HashCb callback);
    void api_get_block(Height height, BlockCb callback);
    void api_get_mining(const Address& a, MiningCb callback);
    void api_get_txcache(TxcacheCb callback);

    void async_set_signed_checkpoint(SignedSnapshot);
    void async_get_blocks(DescriptedBlockRange, getBlocksCb&&);

    void async_stage_request(stage_operation::Operation);

private:
    void close();
    ChainError apply_stage(ChainDBTransaction&& t);
    void workerfun();

    int32_t append_gentx(std::vector<uint8_t>&&);

private:
    void handle_event(MiningAppend&&);
    void handle_event(PutMempool&&);
    void handle_event(GetGrid&&);
    void handle_event(GetBalance&&);
    void handle_event(GetMempool&&);
    void handle_event(LookupTxids&&);
    void handle_event(LookupTxHash&&);
    void handle_event(SetSynced&& e);
    void handle_event(GetHistory&&);
    void handle_event(GetRichlist&&);
    void handle_event(GetHead&&);
    void handle_event(GetHeader&&);
    void handle_event(GetHash&&);
    void handle_event(GetBlock&&);
    void handle_event(GetMining&&);
    void handle_event(GetTxcache&&);
    void handle_event(GetBlocks&&);
    void handle_event(stage_operation::StageSetOperation&&);
    void handle_event(stage_operation::StageAddOperation&&);
    void handle_event(PutMempoolBatch&&);
    void handle_event(SetSignedPin&&);

    std::condition_variable cv;
    ChainDB& db;
    BatchRegistry& batchRegistry;

    // state variables
    chainserver::State state;

    // mutex protected variables
    std::mutex mutex;
    std::queue<Event> events;
    //
    bool haswork = false;
    bool closing = false;
    bool switching = false; // doing chain switch?
    std::jthread worker;
};
