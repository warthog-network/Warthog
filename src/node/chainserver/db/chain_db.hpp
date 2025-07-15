#pragma once

#include "SQLiteCpp/Database.h"
#include "SQLiteCpp/Transaction.h"
#include "api/types/forward_declarations.hpp"
#include "block/block.hpp"
#include "block/block_fwd.hpp"
#include "block/body/container.hpp"
#include "block/chain/history/history.hpp"
#include "block/chain/offsts.hpp"
#include "block/chain/worksum.hpp"
#include "block/id.hpp"
#include "chainserver/db/ids.hpp"
#include "chainserver/transaction_ids.hpp"
#include "db/sqlite_fwd.hpp"
#include "defi/uint64/pool.hpp"
#include "deletion_key.hpp"
#include "general/address_funds.hpp"
#include "general/filelock/filelock.hpp"
#include "general/timestamp.hpp"
#include "order_loader.hpp"
#include "types_fwd.hpp"

struct CreatorToken;
struct AccountToken;
class Batch;
class AssetName;
class CancelId;
class AssetDetail;
struct SignedSnapshot;
class Headerchain;

struct RawUndo : public std::vector<uint8_t> {
};
struct Candle {
    Timestamp timestamp;
    Price_uint64 open;
    Price_uint64 high;
    Price_uint64 low;
    Price_uint64 close;
    Funds_uint64 quantity;
    Funds_uint64 volume;
};

struct PoolData : public defi::Pool_uint64 {
    struct Initializer {
        AssetId assetId;
        Funds_uint64 base;
        Funds_uint64 quote;
        Funds_uint64 shares;
    };
    PoolData(Initializer i)
        : defi::Pool_uint64(i.base.value(), i.quote.value(), i.shares.value())
        , assetId(i.assetId)
    {
    }
    static PoolData zero(AssetId assetId)
    {
        return Initializer {
            .assetId { assetId },
            .base { 0 },
            .quote { 0 },
            .shares { 0 }
        };
    }

    auto asset_id() const { return assetId; }
    auto share_id() const { return assetId.share_id(); }

    defi::PoolLiquidity_uint64 liquidity() const { return { base, quote }; }

private:
    AssetId assetId;
};

struct BlockUndoData {
    Header header;
    BodyData body;
    RawUndo rawUndo;
};

namespace chain_db {
}

namespace chain_db {
class ChainDB {
private:
    using Statement = sqlite::Statement;
    friend class ChainDBTransaction;
    // ids to save additional information in tables
    static constexpr int64_t WORKSUMID = -1;
    static constexpr int64_t SIGNEDPINID = -2;

public:
    // data types
    struct Balance {
        BalanceId balanceId;
        AccountId accountId;
        TokenId tokenId;
        Funds_uint64 balance;
    };
    ChainDB(const std::string& path);
    [[nodiscard]] ChainDBTransaction transaction();
    void insert_account(const AddressView address, AccountId verifyNextStateId);

    void delete_state_from(StateId fromStateId);
    // void setStateBalance(AccountId accountId, Funds balance);
    void insert_consensus(NonzeroHeight height, BlockId blockId, HistoryId historyCursor, uint64_t stateId);

    std::tuple<std::vector<Batch>, HistoryHeights, AccountHeights>
    getConsensusHeaders() const;

    // Consensus Functions
    Worksum get_consensus_work() const;
    void set_consensus_work(const Worksum& ws);
    std::optional<SignedSnapshot> get_signed_snapshot() const;
    void set_signed_snapshot(const SignedSnapshot&);
    [[nodiscard]] std::vector<BlockId> consensus_block_ids(HeightRange) const;

    //////////////////
    // delete schedule functiosn
    [[nodiscard]] DeletionKey delete_consensus_from(NonzeroHeight height);

    void garbage_collect_blocks(DeletionKey);
    [[nodiscard]] DeletionKey schedule_protected_all();
    [[nodiscard]] DeletionKey schedule_protected_part(Headerchain hc, NonzeroHeight fromHeight);
    void protect_stage_assert_scheduled(BlockId id);

    //////////////////
    // Block functions
    // get
    [[nodiscard]] std::optional<BlockId> lookup_block_id(const HashView hash) const;
    [[nodiscard]] std::optional<NonzeroHeight> lookup_block_height(const HashView hash) const;
    [[nodiscard]] std::optional<BlockUndoData> get_block_undo(BlockId id) const;
    [[nodiscard]] std::optional<Block> get_block(BlockId id) const;
    [[nodiscard]] std::optional<std::pair<BlockId, Block>> get_block(HashView hash) const;
    [[nodiscard]] std::optional<BodyData> get_block_body(HashView hash) const;
    // set
    std::pair<BlockId, bool> insert_protect(const Block&);
    void set_block_undo(BlockId id, const std::vector<uint8_t>& undo);

    /////////////////////
    // Order functions
    void prune_candles(Timestamp timestamp);
    void insert_candles_5m(TokenId, const Candle&);
    std::optional<Candle> select_candle_5m(TokenId, Timestamp);
    std::vector<Candle> select_candles_5m(TokenId, Timestamp from, Timestamp to);
    void insert_candles_1h(TokenId, const Candle&);
    std::optional<Candle> select_candle_1h(TokenId, Timestamp);
    std::vector<Candle> select_candles_1h(TokenId, Timestamp from, Timestamp to);

    /////////////////////
    // Order functions
    void insert_order(const chain_db::OrderData&);
    void change_fillstate(const chain_db::OrderFillstate&, bool buy);
    void delete_order(const chain_db::OrderDelete&);
    [[nodiscard]] std::optional<chain_db::OrderData> select_order(TransactionId) const;

    [[nodiscard]] OrderLoaderAscending base_order_loader_ascending(AssetId) const;
    [[nodiscard]] OrderLoaderDescending quote_order_loader_descending(AssetId) const;

    /////////////////////
    // Canceled functions

    void insert_canceled(CancelId cid, AccountId aid, PinHeight ph, NonceId nid);
    void delete_canceled(CancelId cid);

    /////////////////////
    // Account functions
    // get
    [[nodiscard]] std::optional<Address> lookup_address(AccountId id) const;
    [[nodiscard]] Address fetch_address(AccountId id) const;

    /////////////////////
    // Pool functions
    void insert_pool(const PoolData& pool);
    [[nodiscard]] std::optional<PoolData> select_pool(AssetId assetId) const;
    void update_pool(TokenId shareId, Funds_uint64 base, Funds_uint64 quote, Funds_uint64 shares);
    void set_pool_liquidity(AssetId, const defi::PoolLiquidity_uint64&);

    /////////////////////
    // Token fork balance functions
    void insert_token_fork_balance(TokenForkBalanceId, TokenId, TokenForkId, Funds_uint64);
    // bool fork_balance_exists(AccountToken, NonzeroHeight);
    std::optional<std::pair<NonzeroHeight, Funds_uint64>> get_balance_snapshot_after(TokenId tokenId, NonzeroHeight minHeight) const;

    /////////////////////
    // Token functions
    void insert_new_token(const chain_db::AssetData&);
    [[nodiscard]] std::optional<NonzeroHeight> get_latest_fork_height(TokenId, Height);

    [[nodiscard]] std::optional<Balance> get_token_balance(BalanceId id) const;
    [[nodiscard]] std::optional<std::pair<BalanceId, Funds_uint64>> get_balance(AccountId aid, TokenId tid) const;
    [[nodiscard]] Wart get_wart_balance(AccountId aid) const;
    [[nodiscard]] std::optional<AssetDetail> lookup_asset(AssetId) const;
    [[nodiscard]] AssetDetail fetch_asset(AssetId id) const;
    [[nodiscard]] std::optional<AssetDetail> lookup_asset(const AssetHash&) const;
    [[nodiscard]] AssetDetail fetch_asset(const AssetHash&) const;
    void insert_token_balance(AccountToken, Funds_uint64 balance);
    void set_balance(BalanceId, Funds_uint64 balance);
    std::vector<std::pair<TokenId, Funds_uint64>> get_tokens(AccountId, size_t limit);
    [[nodiscard]] api::Richlist lookup_richlist(TokenId, size_t limit) const;
    /////////////////////
    // Transactions functions
    [[nodiscard]] api::Richlist look(size_t N) const;

    ///////////////
    // Deleteschedule functions

    ///////////////
    // direct delete (without delete schedule)
    void delete_bad_block(HashView);

    // txids

    chainserver::TransactionIds fetch_tx_ids(Height) const;

    ///////////////
    // Badblocks functions
    // get
    std::vector<std::pair<Height, Header>>
    getBadblocks() const;
    // set
    void insert_bad_block(NonzeroHeight height, const HeaderView header);

    void insert_history_link(HistoryId parent, HistoryId link);
    HistoryId insertHistory(const HashView hash,
        const std::vector<uint8_t>& data);
    void delete_history_from(NonzeroHeight);
    std::optional<std::pair<history::HistoryVariant, HistoryId>> lookup_history(const HashView hash) const;

    [[nodiscard]] std::vector<std::pair<HistoryId,history::Entry>> lookup_history_range(HistoryId lower, HistoryId upper) const;
    [[nodiscard]] std::optional<history::Entry> lookup_history(HistoryId id) const;
    [[nodiscard]] history::Entry fetch_history(HistoryId id) const;
    void insertAccountHistory(AccountId accountId, HistoryId historyId);
    HistoryId next_history_id() const
    {
        return cache.nextHistoryId;
    }
    auto next_account_id() const { return cache.nextAccountId; }
    auto next_asset_id() const { return cache.nextAssetId; }
    StateId next_state_id() const { return cache.nextStateId; }

    struct AssetLookupTrace { // for debugging
        struct Step {
            TokenId parent;
            Height startHeight;
            std::optional<Height> snapshotHeight;
            Step(TokenId parent, Height startHeight)
                : parent(parent)
                , startHeight(startHeight)
            {
            }
        };
        std::vector<Step> steps;
    };
    [[nodiscard]] std::pair<std::optional<BalanceId>, Funds_uint64> get_token_balance_recursive(AccountId aid, TokenId tid, AssetLookupTrace* trace = nullptr) const;

    //////////////////////////////
    // BELOW METHODS REQUIRED FOR INDEXING NODES
    [[nodiscard]] std::optional<AccountId> lookup_account(const AddressView address) const; // for indexing nodes
    std::vector<std::tuple<HistoryId, history::Entry>> lookup_history_100_desc(AccountId account_id, int64_t beforeId);
    size_t byte_size() const;

private:
    [[nodiscard]] bool schedule_exists(BlockId dk);
    [[nodiscard]] bool consensus_exists(Height h, BlockId dk);

private:
    Filelock fl;
    struct Database : public SQLite::Database {
        Database(const std::string& path);
    } db;
    struct Cache {
        AccountId nextAccountId;
        AssetId nextAssetId;
        StateId nextStateId; // incremental id for tables other than Accounts and Assets
        HistoryId nextHistoryId;
        DeletionKey deletionKey;
        static Cache init(SQLite::Database& db);
    } cache;
    Statement stmtBlockInsert;
    Statement stmtUndoSet;
    mutable Statement stmtBlockGetUndo;
    mutable Statement stmtBlockById;
    mutable Statement stmtBlockByHash;

    // Candles statements
    Statement stmtPruneCandles5m;
    Statement stmtInsertCandles5m;
    Statement stmtSelectCandles5m;
    Statement stmtPruneCandles1h;
    Statement stmtInsertCandles1h;
    Statement stmtSelectCandles1h;

    // Orders statements
    Statement stmtInsertBaseSellOrder;
    Statement stmtUpdateFillBaseSellOrder;
    Statement stmtDeleteBaseSellOrder;
    Statement stmtDeleteBaseSellOrderTxid;
    Statement stmtInsertQuoteBuyOrder;
    Statement stmtUpdateFillQuoteBuyOrder;
    Statement stmtDeleteQuoteBuyOrder;
    Statement stmtDeleteQuoteBuyOrderTxid;
    mutable Statement stmtSelectBaseSellOrderAsc;
    mutable Statement stmtSelectQuoteBuyOrderDesc;
    mutable Statement stmtSelectBaseSell;
    mutable Statement stmtSelectQuoteBuy;

    // Canceled statements
    Statement stmtInsertCanceled;
    Statement stmtDeleteCanceled;

    // Pool statements
    Statement stmtInsertPool;
    mutable Statement stmtSelectPool;
    Statement stmtUpdatePool;
    Statement stmtUpdatePoolLiquidity;

    // TokenForks statements
    Statement stmtTokenForkBalanceInsert;
    mutable Statement stmtTokenForkBalanceEntryExists;
    mutable Statement stmtTokenForkBalanceSelect;
    Statement stmtTokenForkBalancePrune;

    // Token statements
    Statement stmtTokenInsert;
    Statement stmtTokenPrune;
    mutable Statement stmtTokenSelectForkHeight;
    mutable Statement stmtAssetLookup;
    mutable Statement stmtTokenLookupByHash;
    mutable Statement stmtSelectBalanceId;

    // Balance statements
    Statement stmtTokenInsertBalance;
    Statement stmtBalancePrune;
    mutable Statement stmtTokenSelectBalance;
    mutable Statement stmtAccountSelectAssets;
    Statement stmtTokenUpdateBalance;
    mutable Statement stmtTokenSelectRichlist;

    // Consensus table functions
    mutable Statement stmtConsensusHeaders;
    Statement stmtConsensusInsert;
    // Statement stmtConsensusSet;
    Statement stmtConsensusSetProperty;
    mutable Statement stmtConsensusSelect;
    mutable Statement stmtConsensusSelectRange;
    mutable Statement stmtConsensusSelectHistory;
    mutable Statement stmtConsensusHead;
    Statement stmtConsensusDeleteFrom;

    Statement stmtScheduleExists;
    Statement stmtScheduleInsert;
    Statement stmtScheduleBlock;
    Statement stmtScheduleProtected;
    Statement stmtScheduleDelete2;
    Statement stmtScheduleConsensus;
    Statement stmtDeleteGCBlocks;
    Statement stmtDeleteGCRefs;

    Statement stmtAccountsInsert;
    Statement stmtAccountsDeleteFrom;
    Statement stmtBadblockInsert;
    mutable Statement stmtBadblockGet;
    mutable Statement stmtAccountsLookup;
    Statement stmtHistoryLinkInsert;
    Statement stmtHistoryInsert;
    Statement stmtHistoryDeleteFrom;
    mutable Statement stmtHistoryLookup;
    mutable Statement stmtHistoryLookupRange;
    Statement stmtAccountHistoryInsert;
    Statement stmtAccountHistoryDeleteFrom;

    mutable Statement stmtBlockIdSelect;
    mutable Statement stmtBlockHeightSelect;
    Statement stmtBlockDelete;

    mutable Statement stmtAddressLookup;
    mutable Statement stmtHistoryById;
    mutable Statement stmtGetDBSize;
};
class ChainDBTransaction {
public:
    void commit()
    {
        tx.commit();
        commited = true;
    }
    ~ChainDBTransaction()
    {
        if (parent != nullptr && !commited) {
            parent->cache = c;
        }
    }
    ChainDBTransaction(const ChainDBTransaction&) = delete;
    ChainDBTransaction(ChainDBTransaction&& other)
        : parent(other.parent)
        , tx(std::move(other.tx))
        , c(std::move(other.c))
    {
        other.commited = true;
    }

private:
    friend ChainDB;
    ChainDBTransaction(ChainDB& parent)
        : parent(&parent)
        , tx(parent.db)
        , c(parent.cache)
    {
    }
    bool commited = false;
    ChainDB* parent;
    SQLite::Transaction tx;
    ChainDB::Cache c;
};
}
