#pragma once

#include "api/types/forward_declarations.hpp"
#include "block/block.hpp"
#include "block/chain/offsts.hpp"
#include "block/chain/worksum.hpp"
#include "block/id.hpp"
#include "chainserver/transaction_ids.hpp"
#include "deletion_key.hpp"
#include "general/address_funds.hpp"
#include "general/filelock/filelock.hpp"
#include "general/sqlite.hpp"
class ChainDBTransaction;
class Batch;
struct SignedSnapshot;
class Headerchain;
struct RawBody : public std::vector<uint8_t> {
};
struct RawUndo : public std::vector<uint8_t> {
};

class ChainDB {
private:
    using Statement = sqlite::Statement;
    friend class ChainDBTransaction;
    // ids to save additional information in tables
    static constexpr int64_t WORKSUMID = -1;
    static constexpr int64_t SIGNEDPINID = -2;

public:
    ChainDB(const std::string& path);
    [[nodiscard]] ChainDBTransaction transaction();
    void set_balance(AccountId stateId, Funds newbalance)
    {
        stmtStateSetBalance.run(newbalance, stateId);
    };
    void insertStateEntry(const AddressView address, Funds balance,
        AccountId verifyNextStateId);

    void delete_state_from(AccountId fromAccountId);
    // void setStateBalance(AccountId accountId, Funds balance);
    void insert_consensus(NonzeroHeight height, BlockId blockId, HistoryId historyCursor, AccountId accountCursor);
    std::tuple<std::vector<Batch>, HistoryHeights, AccountHeights>
    getConsensusHeaders() const;

    // Consensus Functions
    Worksum get_consensus_work() const;
    void set_consensus_work(const Worksum& ws);
    std::optional<SignedSnapshot> get_signed_snapshot() const;
    void set_signed_snapshot(const SignedSnapshot&);
    [[nodiscard]] std::vector<BlockId> consensus_block_ids(Height begin, Height end) const;

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
    [[nodiscard]] std::optional<std::tuple<Header, RawBody, RawUndo>> get_block_undo(BlockId id) const;
    [[nodiscard]] std::optional<Block> get_block(BlockId id) const;
    [[nodiscard]] std::optional<std::pair<BlockId, Block>> get_block(HashView hash) const;
    // set
    std::pair<BlockId, bool> insert_protect(const Block&);
    void set_block_undo(BlockId id, const std::vector<uint8_t>& undo);

    /////////////////////
    // Account functions
    // get
    [[nodiscard]] std::optional<AddressFunds> lookup_account(AccountId id) const;
    [[nodiscard]] api::Richlist lookup_richlist(uint32_t N) const;
    [[nodiscard]] AddressFunds fetch_account(AccountId id) const;

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

    AccountId next_state_id() const { return AccountId(cache.maxStateId + 1); };
    HistoryId insertHistory(const HashView hash,
        const std::vector<uint8_t>& data);
    void delete_history_from(NonzeroHeight);
    std::optional<std::pair<std::vector<uint8_t>, HistoryId>> lookup_history(const HashView hash);

    std::vector<std::pair<Hash, std::vector<uint8_t>>>
    lookupHistoryRange(HistoryId lower, HistoryId upper);
    void insertAccountHistory(AccountId accountId, HistoryId historyId);
    HistoryId next_history_id() const { return cache.nextHistoryId; }

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
            db.exec("CREATE TABLE IF NOT EXISTS `AccountHistory` (`account_id` "
                    "INTEGER, `history_id` INTEGER, PRIMARY "
                    "KEY(`account_id`,`history_id`)) "
                    "WITHOUT ROWID");

            db.exec("CREATE TABLE IF NOT EXISTS `Blocks` ( `height` INTEGER "
                    "NOT NULL, `header` BLOB NOT NULL, `body` BLOB NOT NULL, "
                    "`undo` BLOB DEFAULT null, `hash` BLOB NOT NULL UNIQUE )");
            db.exec("CREATE TABLE IF NOT EXISTS \"Consensus\" ( `height` INTEGER NOT "
                    "NULL, `block_id` INTEGER NOT NULL, `history_cursor` INTEGER NOT "
                    "NULL, `account_cursor` INTEGER NOT NULL, PRIMARY KEY(`height`) )");
            db.exec(" INSERT OR IGNORE INTO `Consensus` (`height`, "
                    "`block_id`,`history_cursor`, `account_cursor`) "
                    "VALUES "
                    "(-1,x'"
                    "0000000000000000000000000000000000000000000000000000000000"
                    "000000',0,0);");
            db.exec("CREATE TABLE IF NOT EXISTS `State` ( `address` BLOB NOT "
                    "NULL UNIQUE, "
                    "`balance` INTEGER NOT NULL DEFAULT 0 )");
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
        AccountId maxStateId;
        HistoryId nextHistoryId;
        DeletionKey deletionKey;
        static Cache init(SQLite::Database& db);
    } cache;
    Statement stmtBlockInsert;
    Statement stmtUndoSet;
    mutable Statement stmtBlockGetUndo;
    mutable Statement stmtBlockById;
    mutable Statement stmtBlockByHash;

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

    Statement stmtStateInsert;
    Statement stmtStateDeleteFrom;
    Statement stmtStateSetBalance;
    Statement stmtBadblockInsert;
    mutable Statement stmtBadblockGet;
    mutable Statement stmtAccountLookup;
    mutable Statement stmtRichlistLookup;
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
