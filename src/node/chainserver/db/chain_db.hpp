#pragma once

#include "api/types/forward_declarations.hpp"
#include "block/block.hpp"
#include "block/body/order_id.hpp"
#include "block/chain/offsts.hpp"
#include "block/chain/worksum.hpp"
#include "block/id.hpp"
#include "chainserver/transaction_ids.hpp"
#include "defi/token/token.hpp"
#include "deletion_key.hpp"
#include "general/address_funds.hpp"
#include "general/filelock/filelock.hpp"
#include "general/timestamp.hpp"
#include "order_loader.hpp"
#include "statement.hpp"
#include "general/sqlite.hpp"
struct CreatorToken;
struct AccountToken;
class ChainDBTransaction;
class Batch;
class TokenName;
class TokenInfo;
struct SignedSnapshot;
class Headerchain;
struct RawBody : public std::vector<uint8_t> {
};
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

struct PoolData {
    TokenId shareId; // pool shares id
    TokenId tokenId;
    Funds_uint64 base;
    Funds_uint64 quote;
    Funds_uint64 shares;
};

struct BlockUndoData {
    Header header;
    RawBody rawBody;
    RawUndo rawUndo;
};

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
        Funds balance;
    };
    ChainDB(const std::string& path);
    [[nodiscard]] ChainDBTransaction transaction();
    void insert_account(const AddressView address, AccountId verifyNextStateId);

    void delete_state_from(uint64_t fromStateId);
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
    [[nodiscard]] std::optional<ParsedBlock> get_block(BlockId id) const;
    [[nodiscard]] std::optional<std::pair<BlockId, ParsedBlock>> get_block(HashView hash) const;
    // set
    std::pair<BlockId, bool> insert_protect(const ParsedBlock&);
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
    void insert_buy_order(OrderId id, AccountId, TokenId, Funds totalBase, Funds filledBase, Price_uint64 price);
    void insert_quote_order(OrderId id, AccountId, TokenId, Funds totalQuote, Funds filledQuote, Price_uint64 price);

    OrderLoader base_order_loader(TokenId) const;
    OrderLoader quote_order_loader(TokenId) const;

    /////////////////////
    // Account functions
    // get
    [[nodiscard]] std::optional<Address> lookup_address(AccountId id) const;
    [[nodiscard]] Address fetch_address(AccountId id) const;

    /////////////////////
    // Pool functions
    void insert_pool(TokenId shareId, TokenId tokenId);
    std::optional<PoolData> select_pool(TokenId shareIdOrTokenId) const;
    void update_pool(TokenId shareId, Funds_uint64 base, Funds_uint64 quote, Funds_uint64 shares);

    /////////////////////
    // Token fork balance functions
    void insert_token_fork_balance(TokenForkBalanceId, TokenId, TokenForkId, Funds);
    bool fork_balance_exists(AccountToken, NonzeroHeight);
    std::optional<std::pair<NonzeroHeight, Funds>> get_balance_snapshot_after(TokenId tokenId, NonzeroHeight minHeight);

    /////////////////////
    // Token functions
    void insert_new_token(CreatorToken, NonzeroHeight height, TokenName name, TokenHash hash, TokenMintType type);
    [[nodiscard]] std::optional<NonzeroHeight> get_latest_fork_height(TokenId, Height);

    [[nodiscard]] std::optional<Balance> get_token_balance(BalanceId id) const;
    [[nodiscard]] std::optional<std::pair<BalanceId, Funds>> get_balance(AccountToken) const;
    [[nodiscard]] std::optional<TokenInfo> lookup_token(TokenId id) const;
    [[nodiscard]] TokenInfo fetch_token(TokenId id) const;
    void insert_token_balance(AccountToken, Funds balance);
    void set_balance(BalanceId, Funds balance);
    std::vector<std::pair<TokenId, Funds>> get_tokens(AccountId, size_t limit);
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

    auto next_state_id() const
    {
        return cache.nextStateId;
    };
    HistoryId insertHistory(const HashView hash,
        const std::vector<uint8_t>& data);
    void delete_history_from(NonzeroHeight);
    std::optional<std::pair<std::vector<uint8_t>, HistoryId>> lookup_history(const HashView hash);

    std::vector<std::pair<Hash, std::vector<uint8_t>>>
    lookupHistoryRange(HistoryId lower, HistoryId upper);
    void insertAccountHistory(AccountId accountId, HistoryId historyId);
    HistoryId next_history_id() const
    {
        return cache.nextHistoryId;
    }

    struct TokenLookupTrace { // for debugging
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
    std::pair<std::optional<BalanceId>, Funds> get_token_balance_recursive(AccountToken, TokenLookupTrace* trace = nullptr);
    bool write_snapshot_balance(AccountToken, Funds, NonzeroHeight tokenCreationHeight);

    //////////////////////////////
    // BELOW METHODS REQUIRED FOR INDEXING NODES
    std::optional<AccountFunds> lookup_address(const AddressView address) const; // for indexing nodes
    std::vector<std::tuple<HistoryId, Hash, std::vector<uint8_t>>> lookup_history_100_desc(AccountId account_id, int64_t beforeId);
    size_t byte_size() const;

private:
    [[nodiscard]] bool schedule_exists(BlockId dk);
    [[nodiscard]] bool consensus_exists(Height h, BlockId dk);

private:
    SQLite::Database db;
    Filelock fl;
    struct CreateTables {
        CreateTables(SQLite::Database& db)
        {
            db.exec("PRAGMA foreign_keys = ON");
            db.exec("CREATE TABLE IF NOT EXISTS Metadata ("
                    "id INTEGER NOT NULL  "
                    "value BLOB, "
                    "PRIMARY KEY(id))");
            db.exec("CREATE TABLE IF NOT EXISTS `AccountHistory` ("
                    "`account_id` INTEGER, "
                    "`history_id` INTEGER, "
                    "PRIMARY KEY(`account_id`,`history_id`)) "
                    "WITHOUT ROWID");

            db.exec("CREATE TABLE IF NOT EXISTS `Blocks` ( `height` INTEGER "
                    "NOT NULL, `header` BLOB NOT NULL, `body` BLOB NOT NULL, "
                    "`undo` BLOB DEFAULT NULL, `hash` BLOB NOT NULL UNIQUE )");

            // candles 5m
            db.exec("CREATE TABLE IF NOT EXISTS Candles5m ("
                    "tokenId INTEGER NOT NULL, "
                    "timestamp INTEGER NOT NULL, " // start timestamp
                    "open INTEGER NOT NULL, "
                    "high INTEGER NOT NULL, "
                    "low INTEGER NOT NULL, "
                    "close INTEGER NOT NULL, "
                    "quantity INTEGER NOT NULL, " // base amount traded
                    "volume INTEGER NOT NULL, " // quote amount traded
                    "PRIMARY KEY(token_id, timestamp))");
            db.exec("CREATE INDEX IF NOT EXISTS `Candles5mIndex` ON "
                    "`Candles5m` (timestamp)");

            // candles 1h
            db.exec("CREATE TABLE IF NOT EXISTS Candles1h ("
                    "tokenId INTEGER NOT NULL, "
                    "timestamp INTEGER NOT NULL, " // start timestamp
                    "open INTEGER NOT NULL, "
                    "high INTEGER NOT NULL, "
                    "low INTEGER NOT NULL, "
                    "close INTEGER NOT NULL, "
                    "quantity INTEGER NOT NULL, " // base amount traded
                    "volume INTEGER NOT NULL, " // quote amount traded
                    "PRIMARY KEY(token_id, timestamp))");
            db.exec("CREATE INDEX IF NOT EXISTS `Candles1hIndex` ON "
                    "`Candles1h` (timestamp)");

            // Sell orders
            db.exec("CREATE TABLE IF NOT EXISTS SellOrders ("
                    "id INTEGER NOT NULL, "
                    "account_id INTEGER NOT NULL, "
                    "token_id INTEGER NOT NULL, "
                    "totalBase INTEGER NOT NULL, "
                    "filledBase INTEGER NOT NULL, "
                    "price NOT NULL DEFAULT 0, "
                    "PRIMARY KEY(`id`))");
            db.exec("CREATE INDEX IF NOT EXISTS `SellOrderIndex` ON "
                    "`SellOrders` (token_id, price ASC, id ASC)");

            // Buy orders
            db.exec("CREATE TABLE IF NOT EXISTS BuyOrders ("
                    "id INTEGER NOT NULL, "
                    "account_id INTEGER NOT NULL, "
                    "token_id INTEGER NOT NULL, "
                    "totalQuote INTEGER NOT NULL, "
                    "filledQuote INTEGER NOT NULL, "
                    "price NOT NULL DEFAULT 0, "
                    "PRIMARY KEY(`id`))");
            db.exec("CREATE INDEX IF NOT EXISTS `BuyOrderIndex` ON "
                    "`BuyOrders` (token_id, price DESC, id ASC)");

            // Pools
            db.exec("CREATE TABLE IF NOT EXISTS Pools ("
                    "id INTEGER NOT NULL, " // this one is also share id
                    "token_id INTEGER UNIQUE NOT NULL, "
                    "`liquidity_token` INTEGER NOT NULL, "
                    "`liquidity_wart` INTEGER NOT NULL, "
                    "`pool_shares` INTEGER NOT NULL, "
                    "PRIMARY KEY(`id`))");

            // Peg
            db.exec("CREATE TABLE IF NOT EXISTS Peg ("
                    "id INTEGER NOT NULL, "
                    "account_id INTEGER NOT NULL, "
                    "out_token_id INTEGER NOT NULL, "
                    "out_total INTEGER NOT NULL, "
                    "out_paid INTEGER NOT NULL DEFAULT 0, "
                    "in_total INTEGER NOT NULL, "
                    "in_burnt INTEGER NOT NULL DEFAULT 0"
                    "in_token_id INTEGER NOT NULL UNIQUE, "
                    "expiration_height INTEGER NOT NULL, "
                    "PRIMARY KEY(`id`))");
            db.exec("CREATE INDEX IF NOT EXISTS `PegIndex` ON "
                    "`Peg` (expiration_height)");

            // TokenForkBalances
            db.exec("CREATE TABLE IF NOT EXISTS TokenForkBalances ("
                    "`id` INTEGER NOT NULL, "
                    "`account_id` INTEGER NOT NULL, "
                    "`token_id` INTEGER NOT NULL, "
                    "`height` INTEGER NOT NULL, "
                    "`balance` INTEGER NOT NULL, "
                    "PRIMARY KEY(`id`))");
            db.exec("CREATE UNIQUE INDEX IF NOT EXISTS `TokenForkBalancesIndex` "
                    "ON `TokenForks` (account_id, token_id, height)");

            // Tokens
            db.exec("CREATE TABLE IF NOT EXISTS \"Tokens\" ( "
                    "`id` INTEGER DEFAULT NULL, "
                    "`height` INTEGER NOT NULL, "
                    "`owner_account_id` INTEGER NOT NULL, "
                    "`total_supply` INTEGER NOT NULL, "
                    "`group_id` INTEGER NOT NULL, " // group
                    "`parent_id` INTEGER, " // used for forks
                    "`name` TEXT NOT NULL UNIQUE, "
                    "`hash` TEXT NOT NULL UNIQUE, "
                    "`data` BLOB,"
                    "PRIMARY KEY(id))");
            db.exec("CREATE INDEX IF NOT EXISTS `TokensIndex` ON "
                    "`Tokens` (height)");
            db.exec("CREATE INDEX IF NOT EXISTS `TokensParentIndex` ON "
                    "`Tokens` (parent_id, height)");

            db.exec("CREATE TABLE IF NOT EXISTS \"Balances\" (`id` INTEGER NOT NULL, `account_id` INTEGER NOT NULL, `token_id` INTEGER NOT NULL, `balance` INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(`id`))");
            db.exec("CREATE UNIQUE INDEX IF NOT EXISTS `Balance_index` ON "
                    "`Balances` (`account_id` ASC, `token_id` ASC)");
            db.exec("CREATE INDEX IF NOT EXISTS `Balance_index2` ON "
                    "`Balances` (`token_id`, `balance` DESC)");
            db.exec("CREATE TABLE IF NOT EXISTS \"Consensus\" ( `height` INTEGER NOT "
                    "NULL, `block_id` INTEGER NOT NULL, `history_cursor` INTEGER NOT "
                    "NULL, `account_cursor` INTEGER NOT NULL, PRIMARY KEY(`height`) )");
            db.exec(" INSERT OR IGNORE INTO `Consensus` (`height`, "
                    "`block_id`,`history_cursor`, `account_cursor`) "
                    "VALUES "
                    "(-1,x'"
                    "0000000000000000000000000000000000000000000000000000000000"
                    "000000',0,0);");
            db.exec("CREATE TABLE IF NOT EXISTS `Accounts` ( `id` INTEGER NOT NULL, "
                    "`address` BLOB NOT NULL UNIQUE, PRIMARY KEY(`id`))");
            db.exec("CREATE TABLE IF NOT EXISTS `Badblocks` ( `height` INTEGER, "
                    "`header` "
                    "BLOB UNIQUE )");
            db.exec("CREATE TABLE IF NOT EXISTS`Deleteschedule` ( `block_id`	INTEGER NOT NULL, `deletion_key`	INTEGER, PRIMARY KEY(`block_id`))");

            // create indices
            db.exec("CREATE INDEX IF NOT EXISTS `deletion_key` ON `Deleteschedule` ( `deletion_key`)");
            db.exec("CREATE INDEX IF NOT EXISTS `account_history_index` ON "
                    "`AccountHistory` (`history_id` ASC)");
            db.exec("CREATE TABLE IF NOT EXISTS `History` ( `id` INTEGER NOT NULL, "
                    "`hash` BLOB NOT NULL, `data` BLOB NOT NULL, PRIMARY KEY(`id`))");
            db.exec("CREATE INDEX IF NOT EXISTS `history_index` ON "
                    "`History` (`hash` ASC)");
        }
    } createTables;
    struct Cache {
        AccountId nextAccountId;
        TokenId nextTokenId;
        uint64_t nextStateId; // incremental id for tables other than Accounts and Tokens
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
    Statement stmtUpdateCandles5m;
    Statement stmtSelectCandles5m;
    Statement stmtPruneCandles1h;
    Statement stmtInsertCandles1h;
    Statement stmtUpdateCandles1h;
    Statement stmtSelectCandles1h;

    // Orders statements
    Statement stmtInsertBaseSellOrder;
    Statement stmtInsertQuoteBuyOrder;
    mutable Statement stmtSelectBaseSellOrderAsc;
    mutable Statement stmtSelectQuoteBuyOrderDesc;

    // Pool statements
    Statement stmtInsertPool;
    mutable Statement stmtSelectPool;
    Statement stmtUpdatePool;

    // TokenForks statements
    Statement stmtTokenForkBalanceInsert;
    mutable Statement stmtTokenForkBalanceEntryExists;
    mutable Statement stmtTokenForkBalanceSelect;
    Statement stmtTokenForkBalancePrune;

    // Token statements
    Statement stmtTokenInsert;
    Statement stmtTokenPrune;
    mutable Statement stmtTokenMaxSnapshotHeight;
    mutable Statement stmtTokenSelectForkHeight;
    mutable Statement stmtTokenLookup;
    mutable Statement stmtSelectBalanceId;

    // Balance statements
    Statement stmtTokenInsertBalance;
    Statement stmtBalancePrune;
    mutable Statement stmtTokenSelectBalance;
    mutable Statement stmtAccountSelectTokens;
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
    friend class ChainDB;
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
