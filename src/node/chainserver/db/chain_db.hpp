#pragma once

#include "SQLiteCpp/SQLiteCpp.h"
#include "api/types/forward_declarations.hpp"
#include "block/block.hpp"
#include "block/chain/offsts.hpp"
#include "block/chain/worksum.hpp"
#include "block/id.hpp"
#include "chainserver/transaction_ids.hpp"
#include "defi/token/token.hpp"
#include "deletion_key.hpp"
#include "general/address_funds.hpp"
#include "general/filelock/filelock.hpp"
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

struct Column2 : public SQLite::Column {

    operator Hash() const
    {
        return { get_array<32>() };
    }
    operator TokenHash() const
    {
        return static_cast<Hash>(*this);
    }
    operator Height() const
    {
        int64_t h = getInt64();
        if (h < 0) {
            throw std::runtime_error("Database corrupted. Negative height h="
                + std::to_string(h) + " observed");
        }
        return Height(h);
    }
    operator HistoryId() const
    {
        int64_t h = getInt64();
        if (h <= 0) {
            throw std::runtime_error("Database corrupted. HistoryId not positive.");
        }
        return HistoryId { uint64_t(h) };
    }
    template <size_t size>
    std::array<uint8_t, size> get_array() const
    {
        std::array<uint8_t, size> res;
        if (getBytes() != size)
            throw std::runtime_error(
                "Database corrupted, cannot load " + std::to_string(size) + " bytes");
        memcpy(res.data(), getBlob(), size);
        return res;
    }

    std::vector<uint8_t> get_vector() const
    {
        std::vector<uint8_t> res(getBytes());
        memcpy(res.data(), getBlob(), getBytes());
        return res;
    }
    operator std::vector<uint8_t>() const
    {
        return get_vector();
    }
    operator Address() const
    {
        return get_array<20>();
    }
    operator TokenName()
    {
        return TokenName::parse_throw(static_cast<std::string>(*this));
    }
    operator BodyContainer() const
    {
        return { std::vector<uint8_t>(*this) };
    }
    operator Header() const
    {
        return get_array<80>();
        ;
    }
    operator NonzeroHeight() const
    {
        Height h { *this };
        return h.nonzero_throw("Database corrupted, block has height 0");
    }
    operator int64_t() const
    {
        return getInt64();
    }
    operator AccountId() const
    {
        return AccountId(int64_t(getInt64()));
    }
    operator TokenId() const
    {
        return TokenId(int64_t(getInt64()));
    }
    operator IsUint64() const
    {
        return IsUint64(int64_t(getInt64()));
    }
    operator BlockId() const
    {
        return BlockId(getInt64());
    }
    operator Funds() const
    {
        auto v { Funds::from_value(int64_t(getInt64())) };
        if (!v.has_value())
            throw std::runtime_error("Database corrupted, invalid funds");
        return *v;
    }
    operator uint64_t() const
    {
        auto i { getInt64() };
        if (i < 0)
            throw std::runtime_error("Database corrupted, expected nonnegative entry");
        return (uint64_t)i;
    }
    template <typename T>
    operator std::optional<T>() const
    {
        if (isNull())
            return {};
        return static_cast<T>(*this);
    }
};

struct Statement2 : public SQLite::Statement {
    using SQLite::Statement::Statement;

    using SQLite::Statement::bind;
    Column2 getColumn(const int aIndex)
    {
        return { Statement::getColumn(aIndex) };
    }
    void bind(const int index, const Worksum& ws)
    {
        bind(index, ws.to_bytes());
    };
    void bind(const int index, const std::vector<uint8_t>& v)
    {
        SQLite::Statement::bind(index, v.data(), v.size());
    }
    template <size_t N>
    void bind(const int index, std::array<uint8_t, N> a)
    {
        SQLite::Statement::bind(index, a.data(), a.size());
    }
    template <size_t N>
    void bind(const int index, View<N> v)
    {
        SQLite::Statement::bind(index, v.data(), v.size());
    }
    void bind(const int index, Funds f)
    {
        SQLite::Statement::bind(index, (int64_t)f.E8());
    };
    void bind(const int index, uint64_t id)
    {
        assert(id < std::numeric_limits<uint64_t>::max());
        SQLite::Statement::bind(index, (int64_t)id);
    };
    void bind(const int index, IsUint64 id)
    {
        SQLite::Statement::bind(index, (int64_t)id.value());
    };
    void bind(const int index, IsUint32 id)
    {
        SQLite::Statement::bind(index, (int64_t)id.value());
    };
    void bind(const int index, BlockId id)
    {
        SQLite::Statement::bind(index, (int64_t)id.value());
    };
    void bind(const int index, Height id)
    {
        SQLite::Statement::bind(index, (int64_t)id.value());
    };
    template <size_t i>
    void recursive_bind()
    {
    }
    template <size_t i, typename T, typename... Types>
    void recursive_bind(T&& t, Types&&... types)
    {
        bind(i, std::forward<T>(t));
        recursive_bind<i + 1>(std::forward<Types>(types)...);
    }
    template <typename... Types>
    uint32_t run(Types&&... types)
    {
        recursive_bind<1>(std::forward<Types>(types)...);
        auto nchanged = exec();
        reset();
        assert(nchanged >= 0);
        return nchanged;
    }

    // private:
    struct Row {
        Column2 operator[](int index) const
        {
            value_assert();
            return st.getColumn(index);
        }

        template <typename T>
        T get(int index)
        {
            return operator[](index);
        }

        template <size_t N>
        std::array<uint8_t, N> get_array(int index) const
        {
            value_assert();
            return st.getColumn(index);
        }
        std::vector<uint8_t> get_vector(int index) const
        {
            value_assert();
            return st.getColumn(index);
        }

        template <typename T>
        operator std::optional<T>()
        {
            if (!hasValue)
                return {};
            return get<T>(0);
        }
        bool has_value() const { return hasValue; }
        auto process(auto lambda) const
        {
            using ret_t = std::remove_cvref_t<decltype(lambda(*this))>;
            std::optional<ret_t> r;
            if (has_value())
                r = lambda(*this);
            return r;
        }

    private:
        void value_assert() const
        {
            if (!hasValue) {
                throw std::runtime_error(
                    "Database error: trying to access empty result.");
            }
        }
        friend struct Statement2;
        Row(Statement2& st)
            : st(st)
        {
            hasValue = st.executeStep();
        }
        Statement2& st;
        bool hasValue;
    };
    struct SingleResult : public Row {
        using Row::Row;
        ~SingleResult()
        {
            if (hasValue)
                assert(st.executeStep() == false);
            st.reset();
        }
    };

public:
    template <typename... Types>
    [[nodiscard]] SingleResult one(Types&&... types)
    {
        recursive_bind<1>(std::forward<Types>(types)...);
        return SingleResult { *this };
    }

    template <typename... Types, typename Lambda>
    void for_each(Lambda lambda, Types&&... types)
    {
        recursive_bind<1>(std::forward<Types>(types)...);
        while (true) {
            auto r { Row(*this) };
            if (!r.has_value())
                break;
            lambda(r);
        }
        reset();
    }
};
// (`id` INTEGER NOT NULL, `account_id` INTEGER NOT NULL, `token_id` INTEGER NOT NULL, `balance` INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(`id`))

struct BlockUndoData {
    Header header;
    RawBody rawBody;
    RawUndo rawUndo;
};

class ChainDB {
private:
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
    void insert_consensus(NonzeroHeight height, BlockId blockId, HistoryId historyCursor, AccountId accountCursor);

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
    // Account functions
    // get
    [[nodiscard]] std::optional<Address> lookup_address(AccountId id) const;
    [[nodiscard]] Address fetch_address(AccountId id) const;

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
    [[nodiscard]] BalanceId insert_token_balance(TokenId, AccountId, Funds balance);
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

    AccountId next_state_id() const {
        return AccountId(cache.maxStateId + 1); };
    TokenId next_token_id() const {
        return TokenId(cache.maxStateId + 1); };
    HistoryId insertHistory(const HashView hash,
        const std::vector<uint8_t>& data);
    void delete_history_from(NonzeroHeight);
    std::optional<std::pair<std::vector<uint8_t>, HistoryId>> lookup_history(const HashView hash);

    std::vector<std::pair<Hash, std::vector<uint8_t>>>
    lookupHistoryRange(HistoryId lower, HistoryId upper);
    void insertAccountHistory(AccountId accountId, HistoryId historyId);
    HistoryId next_history_id() const {
        return cache.nextHistoryId; }

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

private:
    [[nodiscard]] bool schedule_exists(BlockId dk);
    [[nodiscard]] bool consensus_exists(Height h, BlockId dk);

private:
    SQLite::Database db;
    Filelock fl;
    struct CreateTables {
        CreateTables(SQLite::Database & db)
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

            db.exec("CREATE TABLE IF NOT EXISTS \"Pools\" ( `tokenId` INTEGER "
                    "NOT NULL, `base` INTEGER NOT NULL, `quote` INTEGER NOT NULL, "
                    "PRIMARY KEY(`tokenId`))");

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
            db.exec("CREATE TABLE IF NOT EXISTS Orders ("
                    "id INTEGER NOT NULL, "
                    "account_id INTEGER NOT NULL, "
                    "base_token_id INTEGER NOT NULL UNIQUE, " 
                    "price INTEGER NOT NULL, "
                    "total INTEGER NOT NULL, "
                    "filled INTEGER NOT NULL, "
                    "PRIMARY KEY(`id`))");

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
        }
    } createTables;
    struct Cache {
        uint64_t nextStateId;
        HistoryId nextHistoryId;
        DeletionKey deletionKey;
        static Cache init(SQLite::Database & db);
    } cache;
    Statement2 stmtBlockInsert;
    Statement2 stmtUndoSet;
    mutable Statement2 stmtBlockGetUndo;
    mutable Statement2 stmtBlockById;
    mutable Statement2 stmtBlockByHash;

    // TokenForks statements
    Statement2 stmtTokenForkBalanceInsert;
    mutable Statement2 stmtTokenForkBalanceEntryExists;
    mutable Statement2 stmtTokenForkBalanceSelect;
    Statement2 stmtTokenForkBalancePrune;

    // Token statements
    Statement2 stmtTokenInsert;
    Statement2 stmtTokenPrune;
    mutable Statement2 stmtTokenMaxSnapshotHeight;
    mutable Statement2 stmtTokenSelectForkHeight;
    mutable Statement2 stmtTokenLookup;
    mutable Statement2 stmtSelectBalanceId;

    // Balance statements
    Statement2 stmtTokenInsertBalance;
    Statement2 stmtBalancePrune;
    mutable Statement2 stmtTokenSelectBalance;
    mutable Statement2 stmtAccountSelectTokens;
    Statement2 stmtTokenUpdateBalance;
    mutable Statement2 stmtTokenSelectRichlist;

    // Consensus table functions
    mutable Statement2 stmtConsensusHeaders;
    Statement2 stmtConsensusInsert;
    // Statement2 stmtConsensusSet;
    Statement2 stmtConsensusSetProperty;
    mutable Statement2 stmtConsensusSelect;
    mutable Statement2 stmtConsensusSelectRange;
    mutable Statement2 stmtConsensusSelectHistory;
    mutable Statement2 stmtConsensusHead;
    Statement2 stmtConsensusDeleteFrom;

    Statement2 stmtScheduleExists;
    Statement2 stmtScheduleInsert;
    Statement2 stmtScheduleBlock;
    Statement2 stmtScheduleProtected;
    Statement2 stmtScheduleDelete2;
    Statement2 stmtScheduleConsensus;
    Statement2 stmtDeleteGCBlocks;
    Statement2 stmtDeleteGCRefs;

    Statement2 stmtAccountsInsert;
    Statement2 stmtAccountsDeleteFrom;
    Statement2 stmtBadblockInsert;
    mutable Statement2 stmtBadblockGet;
    mutable Statement2 stmtAccountsLookup;
    Statement2 stmtHistoryInsert;
    Statement2 stmtHistoryDeleteFrom;
    mutable Statement2 stmtHistoryLookup;
    mutable Statement2 stmtHistoryLookupRange;
    Statement2 stmtAccountHistoryInsert;
    Statement2 stmtAccountHistoryDeleteFrom;

    mutable Statement2 stmtBlockIdSelect;
    mutable Statement2 stmtBlockHeightSelect;
    Statement2 stmtBlockDelete;

    mutable Statement2 stmtAddressLookup;
    mutable Statement2 stmtHistoryById;
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
