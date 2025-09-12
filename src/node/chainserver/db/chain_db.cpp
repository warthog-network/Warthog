#include "chain_db.hpp"
#include "api/types/all.hpp"
#include "block/chain/header_chain.hpp"
#include "block/chain/history/history.hpp"
#include "block/header/header_impl.hpp"
#include "block/header/view_inline.hpp"
#include "chainserver/db/ids.hpp"
#include "db/sqlite.hpp"
#include "defi/token/account_token.hpp"
#include "defi/token/info.hpp"
#include "defi/token/token.hpp"
#include "general/address_funds.hpp"
#include "general/hex.hpp"
#include "general/writer.hpp"
#include "global/globals.hpp"
#include "sqlite3.h"
#include "types.hpp"
#include <spdlog/spdlog.h>

namespace {
enum METATYPES { MAXSTATE = 0 };
}

using namespace std::string_literals;

namespace chain_db {
ChainDB::Database::Database(const std::string& path)
    : SQLite::Database([&]() -> auto& {
    spdlog::debug("Opening chain database \"{}\"", path);
    return path; }(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)
{
    exec("PRAGMA foreign_keys = ON");
    exec("CREATE TABLE IF NOT EXISTS Metadata ("
         "id INTEGER NOT NULL  "
         "value BLOB, "
         "PRIMARY KEY(id))");
    exec("CREATE TABLE IF NOT EXISTS `AccountHistory` ("
         "`account_id` INTEGER, "
         "`history_id` INTEGER, "
         "PRIMARY KEY(`account_id`,`history_id`)) "
         "WITHOUT ROWID");

    exec("CREATE TABLE IF NOT EXISTS `Blocks` ( `height` INTEGER "
         "NOT NULL, `header` BLOB NOT NULL, `body` BLOB NOT NULL, "
         "`undo` BLOB DEFAULT NULL, `hash` BLOB NOT NULL UNIQUE )");

    // candles 5m
    exec("CREATE TABLE IF NOT EXISTS Candles5m ("
         "tokenId INTEGER NOT NULL, "
         "timestamp INTEGER NOT NULL, " // start timestamp
         "open INTEGER NOT NULL, "
         "high INTEGER NOT NULL, "
         "low INTEGER NOT NULL, "
         "close INTEGER NOT NULL, "
         "quantity INTEGER NOT NULL, " // base amount traded
         "volume INTEGER NOT NULL, " // quote amount traded
         "PRIMARY KEY(asset_id, timestamp))");
    exec("CREATE INDEX IF NOT EXISTS `Candles5mIndex` ON "
         "`Candles5m` (timestamp)");

    // candles 1h
    exec("CREATE TABLE IF NOT EXISTS Candles1h ("
         "tokenId INTEGER NOT NULL, "
         "timestamp INTEGER NOT NULL, " // start timestamp
         "open INTEGER NOT NULL, "
         "high INTEGER NOT NULL, "
         "low INTEGER NOT NULL, "
         "close INTEGER NOT NULL, "
         "quantity INTEGER NOT NULL, " // base amount traded
         "volume INTEGER NOT NULL, " // quote amount traded
         "PRIMARY KEY(asset_id, timestamp))");
    exec("CREATE INDEX IF NOT EXISTS `Candles1hIndex` ON "
         "`Candles1h` (timestamp)");

    // Sell orders
    exec("CREATE TABLE IF NOT EXISTS SellOrders ("
         "id INTEGER NOT NULL, "
         "account_id INTEGER NOT NULL, "
         "pin_height INTEGER NOT NULL, "
         "nonce_id INTEGER NOT NULL, "
         "asset_id INTEGER NOT NULL, "
         "totalBase INTEGER NOT NULL, "
         "filledBase INTEGER NOT NULL, "
         "limit NOT NULL DEFAULT 0, "
         "PRIMARY KEY(`id`))");
    exec("CREATE INDEX IF NOT EXISTS `SellOrderIndex` ON "
         "`SellOrders` (asset_id, limit ASC, id ASC)");
    exec("CREATE UNIQUE INDEX IF NOT EXISTS `SellOrderAccountIndex` ON "
         "`SellOrders` (account_id, pin_height, nonce_id)");

    // Buy orders
    exec("CREATE TABLE IF NOT EXISTS BuyOrders ("
         "id INTEGER NOT NULL, "
         "account_id INTEGER NOT NULL, "
         "pin_height INTEGER NOT NULL, "
         "nonce_id INTEGER NOT NULL, "
         "asset_id INTEGER NOT NULL, "
         "totalQuote INTEGER NOT NULL, "
         "filledQuote INTEGER NOT NULL, "
         "limit NOT NULL DEFAULT 0, "
         "PRIMARY KEY(`id`))");
    exec("CREATE INDEX IF NOT EXISTS `BuyOrderIndex` ON "
         "`BuyOrders` (asset_id, limit DESC, id ASC)");
    exec("CREATE UNIQUE INDEX IF NOT EXISTS `BuyOrderAccountIndex` ON "
         "`BuyOrders` (account_id, pin_height, nonce_id)");

    exec("CREATE TABLE IF NOT EXISTS Canceled ("
         "id INTEGER NOT NULL, "
         "account_id INTEGER NOT NULL, "
         "pin_height INTEGER NOT NULL, "
         "nonce_id INTEGER NOT NULL, "
         "PRIMARY KEY(`id`))");
    exec("CREATE INDEX IF NOT EXISTS `CanceledIndex` ON "
         "`Canceled` (accountId, pin_height, nonce_id)");

    // Pools
    exec("CREATE TABLE IF NOT EXISTS Pools ("
         "asset_id INTEGER NOT NULL, "
         "`liquidity_token` INTEGER NOT NULL, "
         "`liquidity_wart` INTEGER NOT NULL, "
         "`pool_shares` INTEGER NOT NULL, "
         "PRIMARY KEY(`asset_id`))");

    // Peg
    exec("CREATE TABLE IF NOT EXISTS Peg ("
         "id INTEGER NOT NULL, "
         "account_id INTEGER NOT NULL, "
         "out_asset_id INTEGER NOT NULL, "
         "out_total INTEGER NOT NULL, "
         "out_paid INTEGER NOT NULL DEFAULT 0, "
         "in_total INTEGER NOT NULL, "
         "in_burnt INTEGER NOT NULL DEFAULT 0"
         "in_asset_id INTEGER NOT NULL UNIQUE, "
         "expiration_height INTEGER NOT NULL, "
         "PRIMARY KEY(`id`))");
    exec("CREATE INDEX IF NOT EXISTS `PegIndex` ON "
         "`Peg` (expiration_height)");

    // TokenForkBalances
    exec("CREATE TABLE IF NOT EXISTS TokenForkBalances ("
         "`id` INTEGER NOT NULL, "
         "`account_id` INTEGER NOT NULL, "
         "`asset_id` INTEGER NOT NULL, "
         "`height` INTEGER NOT NULL, "
         "`balance` INTEGER NOT NULL, "
         "PRIMARY KEY(`id`))");
    exec("CREATE UNIQUE INDEX IF NOT EXISTS `TokenForkBalancesIndex` "
         "ON `TokenForks` (account_id, asset_id, height)");

    // Assets
    exec("CREATE TABLE IF NOT EXISTS \"Assets\" ( "
         "`id` INTEGER DEFAULT NULL, "
         "`hash` TEXT NOT NULL UNIQUE, "
         "`name` TEXT NOT NULL UNIQUE, "
         "`precision` INTEGER NOT NULL UNIQUE, "
         "`height` INTEGER NOT NULL, "
         "`owner_account_id` INTEGER NOT NULL, "
         "`total_supply` INTEGER NOT NULL, "
         "`group_id` INTEGER NOT NULL, " // group
         "`parent_id` INTEGER, " // used for forks
         "`data` BLOB,"
         "PRIMARY KEY(id))");
    exec("CREATE INDEX IF NOT EXISTS `AssetsIndex` ON "
         "`Assets` (height)");
    exec("CREATE INDEX IF NOT EXISTS `AssetsParentIndex` ON "
         "`Assets` (parent_id, height)");

    exec("CREATE TABLE IF NOT EXISTS \"Balances\" (`id` INTEGER NOT NULL, `account_id` INTEGER NOT NULL, `token_id` INTEGER NOT NULL, `balance` INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(`id`))");
    exec("CREATE UNIQUE INDEX IF NOT EXISTS `Balance_index` ON "
         "`Balances` (`account_id` ASC, `token_id` ASC)");
    exec("CREATE INDEX IF NOT EXISTS `Balance_index2` ON "
         "`Balances` (`token_id`, `balance` DESC)");
    exec("CREATE TABLE IF NOT EXISTS \"Consensus\" ( `height` INTEGER NOT "
         "NULL, `block_id` INTEGER NOT NULL, `history_cursor` INTEGER NOT "
         "NULL, `account_cursor` INTEGER NOT NULL, PRIMARY KEY(`height`) )");
    exec(" INSERT OR IGNORE INTO `Consensus` (`height`, "
         "`block_id`,`history_cursor`, `account_cursor`) "
         "VALUES "
         "(-1,x'"
         "0000000000000000000000000000000000000000000000000000000000"
         "000000',0,0);");
    exec("CREATE TABLE IF NOT EXISTS `Accounts` ( `id` INTEGER NOT NULL, "
         "`address` BLOB NOT NULL UNIQUE, PRIMARY KEY(`id`))");
    exec("CREATE TABLE IF NOT EXISTS `Badblocks` ( `height` INTEGER, "
         "`header` "
         "BLOB UNIQUE )");
    exec("CREATE TABLE IF NOT EXISTS`Deleteschedule` ( `block_id`	INTEGER NOT NULL, `deletion_key`	INTEGER, PRIMARY KEY(`block_id`))");

    // create indices
    exec("CREATE INDEX IF NOT EXISTS `deletion_key` ON `Deleteschedule` ( `deletion_key`)");
    exec("CREATE INDEX IF NOT EXISTS `account_history_index` ON "
         "`AccountHistory` (`history_id` ASC)");

    exec("CREATE TABLE IF NOT EXISTS `FillLink` ( "
         "`id` INTEGER NOT NULL, " // historyId of fills
         "`link` INTEGER NOT NULL, " // history id of order creation
         " PRIMARY KEY(`id`))");
    exec("CREATE UNIQUE INDEX IF NOT EXISTS `FillLinkIndex` ON "
         "`FillLinks` (link, id)");

    exec("CREATE TABLE IF NOT EXISTS `History` ( `id` INTEGER NOT NULL, "
         "`hash` BLOB NOT NULL, `data` BLOB NOT NULL, PRIMARY KEY(`id`))");
    exec("CREATE INDEX IF NOT EXISTS `history_index` ON "
         "`History` (`hash` ASC)");
}
ChainDB::Cache ChainDB::Cache::init(SQLite::Database& db)
{
    auto get_int64 {
        [&db](const std::string& s) { return int64_t(db.execAndGet(s).getInt64()); }
    };

    auto nextStateId = get_int64("SELECT COALESCE(0,value)+1 FROM Metadata WHERE key=" + std::to_string(METATYPES::MAXSTATE));
    auto nextAccountId = AccountId(get_int64("SELECT coalesce(max(ROWID),0) + 1 FROM `Accounts`"));
    auto nextAssetId = AssetId(get_int64("SELECT coalesce(max(ROWID),0) + 1 FROM `Assets`"));

    int64_t hid = db.execAndGet("SELECT coalesce(max(id)+1,1) FROM History")
                      .getInt64();
    if (hid < 0)
        throw std::runtime_error("Database corrupted, negative history id.");
    return {
        .nextAccountId { nextAccountId },
        .nextAssetId { nextAssetId },
        .nextStateId { nextStateId },
        .nextHistoryId = HistoryId { uint64_t(hid) },
        .deletionKey { 2 }

    };
}

ChainDBTransaction ChainDB::transaction()
{
    return ChainDBTransaction(*this);
}
ChainDB::ChainDB(const std::string& path)
    : fl(path)
    , db(path)
    , cache(Cache::init(db))
    , stmtBlockInsert(db, "INSERT INTO \"Blocks\" ( `height`, `header`, `body` "
                          ", `hash`) VALUES (?,?,?,?)")
    , stmtUndoSet(db, "UPDATE \"Blocks\" SET `undo`=? WHERE `ROWID`=?")
    , stmtBlockGetUndo(
          db, "SELECT `header`,`body`, `undo` FROM \"Blocks\" WHERE `ROWID`=?")
    , stmtBlockById(
          db, "SELECT `height`, `header`, `body` FROM \"Blocks\" WHERE `ROWID`=?;")
    , stmtBlockByHash(
          db, "SELECT ROWID, `height`, `header`, `body` FROM \"Blocks\" WHERE `hash`=?;")
    , stmtPruneCandles5m(db, "DELETE FROM Candles5m WHERE timestamp >=?")
    , stmtInsertCandles5m(db, "INSERT OR REPLACE INTO Candles5m (tokenId, timestamp, open, high, low, close, quantity, volume) VALUES (?,?,?,?,?,?,?,?)")
    , stmtSelectCandles5m(db, "SELECT (timestamp, open, high, low, close, quantity, volume) FROM Candles5m WHERE asset_id=? AND timestamp>=? AND timestamp<=?")
    , stmtPruneCandles1h(db, "DELETE FROM Candles1h WHERE timestamp >=?")
    , stmtInsertCandles1h(db, "INSERT OR REPLACE INTO Candles1h (tokenId, timestamp, open, high, low, close, quantity, volume) VALUES (?,?,?,?,?,?,?,?)")
    , stmtSelectCandles1h(db, "SELECT (timestamp, open, high, low, close, quantity, volume) FROM Candles1h WHERE asset_id=? AND timestamp>=? AND timestamp<=?")

    , stmtInsertBaseSellOrder(db, "INSERT INTO SellOrders (id, account_id, pin_height, nonce_id, asset_id, totalBase, filledBase, limit) VALUES(?,?,?,?,?,?,?,?)")
    , stmtUpdateFillBaseSellOrder(db, "UPDATE SellOrders SET filledBase = ? WHERE id = ?")
    , stmtDeleteBaseSellOrder(db, "DELETE FROM SellOrders WHERE id = ?")
    , stmtDeleteBaseSellOrderTxid(db, "DELETE FROM SellOrders WHERE account_id = ? AND pin_height = ? AND nonce_id = ?")
    , stmtInsertQuoteBuyOrder(db, "INSERT INTO BuyOrders (id, account_id, pin_height, nonce_id, asset_id, totalQuote, filledQuote, limit) VALUES(?,?,?,?,?,?,?,?)")
    , stmtUpdateFillQuoteBuyOrder(db, "UPDATE BuyOrders SET filledQuote = ? WHERE id = ?")
    , stmtDeleteQuoteBuyOrder(db, "DELETE FROM BuyOrders WHERE id = ?")
    , stmtDeleteQuoteBuyOrderTxid(db, "DELETE FROM BuyOrders WHERE account_id = ? AND pin_height = ? AND nonce_id = ?")
    , stmtSelectBaseSellOrderAsc(db, "SELECT (id, account_id, pin_height, nonce_id, totalBase, filledBase, limit) FROM SellOrders WHERE asset_id=? ORDER BY limit ASC, id ASC")
    , stmtSelectQuoteBuyOrderDesc(db, "SELECT (id, account_id, pin_height, nonce_id, totalQuote, filledQuote, limit) FROM BuyOrders WHERE asset_id=? ORDER BY limit DESC, id ASC")
    , stmtSelectBaseSell(db, "SELECT (id, asset_id, totalBase, filledBase, limit) FROM SellOrders WHERE account_id = ? AND pin_height = ? AND nonce_id = ?")
    , stmtSelectQuoteBuy(db, "SELECT (id, asset_id, totalQuote, filledQuote, limit) FROM BuyOrders WHERE account_id = ? AND pin_height = ? AND nonce_id = ?")
    , stmtInsertCanceled(db, "INSERT INTO Canceled (id, account_id, pin_height, nonce_id) VALUES (?,?,?,?)")
    , stmtDeleteCanceled(db, "DELETE FROM Canceled WHERE id = ?")
    , stmtInsertPool(db, "INSERT INTO Pools (asset_id, pool_wart, pool_token, pool_shares) VALUES (?,?,?,?)")
    , stmtSelectPool(db, "SELECT (asset_id, liquidity_token, liquidity_wart, pool_shares) FROM Pools WHERE asset_id=?")
    , stmtUpdatePool(db, "UPDATE Pools SET liquidity_base=?, liquidity_quote=?, pool_shares=? WHERE asset_id=?")
    , stmtUpdatePoolLiquidity(db, "UPDATE Pools SET liquidity_base=?, liquidity_quote=? WHERE asset_id=?")
    , stmtTokenForkBalanceInsert(db, "INSERT INTO TokenForkBalances "
                                     "(id, account_id, token_id, height, balance) "
                                     "VALUES (?,?,?,?)")
    , stmtTokenForkBalanceEntryExists(db, "SELECT 1 FROM TokenForkBalances "
                                          "WHERE account_id=? "
                                          "AND token_id=? "
                                          "AND height=?")
    , stmtTokenForkBalanceSelect(db, "SELECT height, balance FROM `TokenForkBalances` WHERE token_id=? height>=? ORDER BY height ASC LIMIT 1")

    , stmtTokenForkBalancePrune(db, "DELETE FROM TokenForkBalances WHERE id>=?")

    , stmtAssetInsert(db, "INSERT INTO `Assets` ( `id`, `hash, `name`, `precision`, `height`, `owner_account_id`, total_supply, group_id, parent_id, data) VALUES (?,?,?,?,?,?,?,?,?,?)")
    , stmtTokenPrune(db, "DELETE FROM Assets WHERE id>=?")
    , stmtTokenSelectForkHeight(db, "SELECT height FROM Assets WHERE parent_id=? AND height>=? ORDER BY height DESC LIMIT 1")
    , stmtAssetLookup(db, "SELECT (id, hash, name, precision, height, owner_account_id, total_supply, group_id, parent_id) FROM Assets WHERE `id`=?")
    , stmtTokenLookupByHash(db, "SELECT (id, hash, name, precision, height, owner_account_id, total_supply, group_id, parent_id) FROM Assets WHERE `hash`=?")
    , stmtSelectBalanceId(db, "SELECT `account_id`, `token_id`, `balance` FROM `Balances` WHERE `id`=?")
    , stmtTokenInsertBalance(db, "INSERT INTO `Balances` ( id, `token_id`, `account_id`, `balance`) VALUES (?,?,?,?)")
    , stmtBalancePrune(db, "DELETE FROM Balances WHERE id>=?")
    , stmtTokenSelectBalance(db, "SELECT `id`, `balance` FROM `Balances` WHERE `token_id`=? AND `account_id`=?")
    , stmtAccountSelectAssets(db, "SELECT `token_id`, `balance` FROM `Balances` WHERE `account_id`=? LIMIT ?")
    , stmtTokenUpdateBalance(db, "UPDATE `Balances` SET `balance`=? WHERE `id`=?")
    , stmtTokenSelectRichlist(db, "SELECT `address`, `balance` FROM `Balances` JOIN `Accounts` on Accounts.id = Balances.account_id WHERE `token_id`=? ORDER BY `balance` DESC LIMIT ?")
    , stmtConsensusHeaders(db, "SELECT c.height, c.history_cursor, c.account_cursor, b.header "
                               "FROM `Blocks` b JOIN `Consensus` c ON "
                               "b.ROWID=c.block_id ORDER BY c.height ASC;")
    , stmtConsensusInsert(db, "INSERT INTO \"Consensus\" ( `height`, "
                              "`block_id`, `history_cursor`, `account_cursor`) VALUES (?,?,?,?)")
    , stmtConsensusSetProperty(
          db, "INSERT OR REPLACE INTO \"Consensus\" (`height`,`block_id`, `history_cursor`, `account_cursor`) VALUES (?,?,0,0)")
    , stmtConsensusSelect(
          db, "SELECT `block_id` FROM \"Consensus\" WHERE `height`=?")
    , stmtConsensusSelectRange(
          db, "SELECT `block_id` FROM \"Consensus\" WHERE `height`>=? AND "
              "`height` <? ORDER BY height ASC")
    , stmtConsensusSelectHistory(
          db, "SELECT `history_cursor` FROM \"Consensus\" WHERE `height`=?")
    , stmtConsensusHead(
          db, "SELECT `height`, `block_id` FROM \"Consensus\" ORDER BY "
              "`height` DESC LIMIT 1;")
    , stmtConsensusDeleteFrom(db, "DELETE FROM `Consensus` WHERE `height`>=?")

    , stmtScheduleExists(db, "SELECT EXISTS(SELECT 1 FROM `Deleteschedule` WHERE `block_id`=?)")
    , stmtScheduleInsert(db, "INSERT INTO `Deleteschedule` (`block_id`,`deletion_key`) VALUES (?,?)")
    , stmtScheduleBlock(db, "UPDATE `Deleteschedule` SET `deletion_key`=? WHERE `block_id` = ?")
    , stmtScheduleProtected(db, "UPDATE `Deleteschedule` SET `deletion_key`=? WHERE `deletion_key` = 0")
    , stmtScheduleDelete(db, "DELETE FROM `Deleteschedule` WHERE `block_id` = ?")
    , stmtScheduleConsensus(db, "REPLACE INTO `Deleteschedule` (block_id,deletion_key) SELECT block_id, ? FROM Consensus WHERE height >= ?")

    , stmtDeleteGCBlocks(
          db, "DELETE FROM `Blocks` WHERE ROWID IN (SELECT `block_id`  FROM "
              "`Deleteschedule` WHERE `deletion_key`<=? AND `deletion_key` > 0 )")
    , stmtDeleteGCRefs(db, "DELETE  FROM `Deleteschedule` WHERE `deletion_key`<=? AND `deletion_key` > 0")

    , stmtAccountsInsert(db, "INSERT INTO `Accounts` ( `id`, `address`"
                             ") VALUES (?,?)")
    , stmtAccountsDeleteFrom(db, "DELETE FROM `Accounts` WHERE `id`>=?")

    , stmtBadblockInsert(
          db, "INSERT INTO `Badblocks` (`height`, `header`) VALUES (?,?)")
    , stmtBadblockGet(db, "SELECT `height`, `header` FROM `Badblocks`")

    , stmtAccountsLookup(db, "SELECT `Address` FROM `Accounts` WHERE id=?")
    , stmtHistoryLinkInsert(db, "INSERT INTO FillLinks (id,link) VALUES (?,?)")
    , stmtHistoryInsert(db, "INSERT INTO `History` (`id`,`hash`, `data`"
                            ") VALUES (?,?,?)")
    , stmtHistoryDeleteFrom(db, "DELETE FROM `History` WHERE `id`>=?")
    , stmtHistoryLookup(db,
          "SELECT `id`, `data` FROM `History` WHERE `hash`=?")
    , stmtHistoryLookupRange(db,
          "SELECT `id`, `hash`, `data` FROM `History` WHERE `id`>=? AND`id`<?")
    , stmtAccountHistoryInsert(db, "INSERT OR IGNORE INTO `AccountHistory` "
                                   "(`account_id`,`history_id`) VALUES (?,?)")
    , stmtAccountHistoryDeleteFrom(
          db, "DELETE FROM `AccountHistory` WHERE `history_id`>=?")
    , stmtBlockIdSelect(
          db, "SELECT `ROWID` FROM `Blocks` WHERE `hash`=?")
    , stmtBlockHeightSelect(
          db, "SELECT `height` FROM `Blocks` WHERE `hash`=?")
    , stmtBlockDelete(db, "DELETE FROM `Blocks` WHERE ROWID = ?")

    // BELOW STATEMENTS REQUIRED FOR INDEXING NODES
    //
    , stmtAddressLookup(db, "SELECT `ROWID`, FROM `Accounts` JOIN `Balances` on Accounts WHERE `address`=?")
    , stmtHistoryById(db, "SELECT h.id, `hash`,`data` FROM `History` `h` JOIN "
                          "`AccountHistory` `ah` ON h.id=`ah`.history_id WHERE "
                          "ah.`account_id`=? AND h.id<? ORDER BY h.id DESC LIMIT 100")
    , stmtGetDBSize(db, "SELECT page_count * page_size FROM pragma_page_count(), pragma_page_size();")
{

    //
    // Do DELETESCHEDULE cleanup
    db.exec("UPDATE `Deleteschedule` SET `deletion_key`=1");
}

void ChainDB::insert_account(const AddressView address, AccountId verifyNextAccountId)
{
    if (cache.nextAccountId != verifyNextAccountId)
        throw std::runtime_error("Internal error, state id inconsistent.");
    stmtAccountsInsert.run(cache.nextStateId, address);
    cache.nextStateId++;
}

void ChainDB::delete_state_from(StateId fromStateId)
{
    assert(fromStateId.value() > 0);
    if (cache.nextStateId <= fromStateId) {
        spdlog::error("BUG: Deleting nothing, fromAccountId = {} >= {} = cache.maxAccountId", fromStateId.value(), cache.nextStateId.value());
    } else {
        cache.nextStateId = fromStateId;
        stmtAccountsDeleteFrom.run(fromStateId);
        stmtTokenForkBalancePrune.run(fromStateId);
        stmtTokenPrune.run(fromStateId);
        stmtBalancePrune.run(fromStateId);
    }
}

void ChainDB::insert_consensus(NonzeroHeight height, BlockId blockId, HistoryId historyCursor, uint64_t stateId)
{
    stmtConsensusInsert.run(height, blockId, historyCursor, stateId);
    stmtScheduleDelete.run(blockId);
}

Worksum ChainDB::get_consensus_work() const
{
    auto o { stmtConsensusSelect.one(WORKSUMID) };
    if (!o.has_value()) {
        throw std::runtime_error("Database corrupted. No worksum entry");
    }
    return o[0];
}
void ChainDB::set_consensus_work(const Worksum& ws)
{
    stmtConsensusSetProperty.run(WORKSUMID, ws);
}

std::optional<SignedSnapshot> ChainDB::get_signed_snapshot() const
{
    auto o { stmtConsensusSelect.one(SIGNEDPINID) };
    if (!o.has_value()) {
        return {};
    }
    std::vector<uint8_t> v { o.get_vector(0) };
    Reader r(v);
    try {
        return SignedSnapshot(r);
    } catch (Error e) {
        throw std::runtime_error("Database corrupted. Signed snapshot invalid: "s + std::string(e.strerror()));
    }
}

void ChainDB::set_signed_snapshot(const SignedSnapshot& ss)
{
    std::vector<uint8_t> v(SignedSnapshot::binary_size);
    Writer w(v);
    w << ss;
    stmtConsensusSetProperty.run(SIGNEDPINID, v);
}

std::vector<BlockId> ChainDB::consensus_block_ids(HeightRange range) const
{
    auto out { stmtConsensusSelectRange.all([&](const sqlite::Row& r) {
        return BlockId { r[0] };
    },
        range.hbegin, range.hend) };
    if (out.size() != range.hend - range.hbegin)
        throw std::runtime_error("Cannot find block ids in database: " + std::to_string(range.hbegin.value()) + "-" + std::to_string(range.hend.value()));
    return out;
}

void ChainDB::garbage_collect_blocks(DeletionKey dk)
{
    stmtDeleteGCBlocks.run(dk.value());
    stmtDeleteGCRefs.run(dk.value());
}

DeletionKey ChainDB::schedule_protected_all()
{
    auto dk { cache.deletionKey++ };
    stmtScheduleProtected.run(dk.value());
    return dk;
}

bool ChainDB::schedule_exists(BlockId id)
{
    return stmtScheduleExists.one(id.value()).get<int64_t>(0) != 0;
}
bool ChainDB::consensus_exists(Height h, BlockId dk)
{
    return stmtConsensusSelect.one(h).get<uint64_t>(0) == dk.value();
}

DeletionKey ChainDB::schedule_protected_part(Headerchain hc, NonzeroHeight fromHeight)
{
    auto dk { cache.deletionKey++ };
    for (auto h { fromHeight }; h < hc.length(); ++h) {
        auto id { lookup_block_id(hc.get_hash(h).value()) };
        assert(stmtScheduleBlock.run(dk.value(), id.value()) == 1);
    }
    stmtScheduleProtected.run(dk.value());
    return dk;
}

void ChainDB::protect_stage_assert_scheduled(BlockId id)
{
    assert(schedule_exists(id));
    assert(stmtScheduleBlock.run(0, id) == 1);
}

DeletionKey ChainDB::delete_consensus_from(NonzeroHeight height)
{
    auto dk { cache.deletionKey++ };
    stmtScheduleConsensus.run(dk.value(), height);
    stmtConsensusDeleteFrom.run(height);
    return dk;
}

std::optional<Block> ChainDB::get_block(BlockId id) const
{
    auto o { stmtBlockById.one(id) };
    if (!o.has_value())
        return {};
    try {
        return Block(o[0], Header(o[1]), BodyData(o[2]));
    } catch (...) {
        throw std::runtime_error("Cannot load block with id " + std::to_string(id.value()) + ". ");
    }
}

std::optional<BodyData> ChainDB::get_block_body(HashView hash) const
{
    auto o = stmtBlockByHash.one(hash);
    if (!o.has_value())
        return {};
    try {
        return BodyData(std::vector<uint8_t>(o[3]));
    } catch (...) {
        throw std::runtime_error("Cannot load block with hash " + serialize_hex(hash) + ".");
    }
}

std::optional<std::pair<BlockId, Block>> ChainDB::get_block(HashView hash) const
{
    auto o = stmtBlockByHash.one(hash);
    if (!o.has_value())
        return {};
    try {
        return std::pair<BlockId, Block> {
            o[0],
            { o[1], Header(o[2]), BodyData(o[3]) }
        };
    } catch (...) {
        throw std::runtime_error("Cannot load block with hash " + serialize_hex(hash) + ".");
    }
}

std::pair<BlockId, bool> ChainDB::insert_protect(const Block& b)
{
    auto hash { b.header.hash() };

    auto blockId { lookup_block_id(hash) };
    if (blockId.has_value()) {
        assert(schedule_exists(*blockId) || consensus_exists(b.height, *blockId));
        return { blockId.value(), false };
    } else {
        stmtBlockInsert.run(b.height, b.header, b.body.data, hash);
        auto lastId { db.getLastInsertRowid() };
        stmtScheduleInsert.run(lastId, 0);
        return { BlockId(lastId), true };
    }
}

std::optional<BlockUndoData>
ChainDB::get_block_undo(BlockId id) const
{
    return stmtBlockGetUndo.one(id).process([](auto& a) {
        {
            return BlockUndoData {
                .header = a[0],
                .body = a.get_vector(1),
                .rawUndo = { a.get_vector(2) }
            };
        }
    });
}

void ChainDB::set_block_undo(BlockId id, const std::vector<uint8_t>& undo)
{
    stmtUndoSet.run(undo, id);
}

void ChainDB::prune_candles(Timestamp timestamp)
{
    stmtPruneCandles5m.run(timestamp);
    stmtPruneCandles1h.run(timestamp);
}

void ChainDB::insert_candles_5m(TokenId tid, const Candle& c)
{
    stmtInsertCandles5m.run(tid, c.timestamp, c.open, c.high, c.low, c.close, c.quantity, c.volume);
}

std::optional<Candle> ChainDB::select_candle_5m(TokenId tid, Timestamp ts)
{
    return stmtSelectCandles5m.one(tid, ts, ts).process([](auto& o) {
        return Candle {
            .timestamp = static_cast<int64_t>(o[0]),
            .open = o[1],
            .high = o[2],
            .low = o[3],
            .close = o[4],
            .quantity = o[5],
            .volume = o[6],
        };
    });
}

std::vector<Candle> ChainDB::select_candles_5m(TokenId tid, Timestamp from, Timestamp to)
{
    return stmtSelectCandles5m.all(
        [](const auto& o) {
            return Candle {
                .timestamp = static_cast<int64_t>(o[0]),
                .open = o[1],
                .high = o[2],
                .low = o[3],
                .close = o[4],
                .quantity = o[5],
                .volume = o[6],
            };
        },
        tid, from, to);
}

void ChainDB::insert_candles_1h(TokenId tid, const Candle& c)
{
    stmtInsertCandles1h.run(tid, c.timestamp, c.open, c.high, c.low, c.close, c.quantity, c.volume);
}

std::optional<Candle> ChainDB::select_candle_1h(TokenId tid, Timestamp ts)
{
    return stmtSelectCandles1h.one(tid, ts, ts).process([](auto& o) {
        return Candle {
            .timestamp = static_cast<int64_t>(o[0]),
            .open = o[1],
            .high = o[2],
            .low = o[3],
            .close = o[4],
            .quantity = o[5],
            .volume = o[6],
        };
    });
}

std::vector<Candle> ChainDB::select_candles_1h(TokenId tid, Timestamp from, Timestamp to)
{
    return stmtSelectCandles1h.all([](const auto& o) {
        return Candle {
            .timestamp = static_cast<int64_t>(o[0]),
            .open = o[1],
            .high = o[2],
            .low = o[3],
            .close = o[4],
            .quantity = o[5],
            .volume = o[6],
        };
    },
        tid, from, to);
}

void ChainDB::insert_order(const chain_db::OrderData& o)
{
    if (o.buy)
        stmtInsertQuoteBuyOrder.run(o.id, o.txid.accountId, o.txid.pinHeight, o.txid.nonceId, o.aid, o.total, o.filled, o.limit);
    else
        stmtInsertBaseSellOrder.run(o.id, o.txid.accountId, o.txid.pinHeight, o.txid.nonceId, o.aid, o.total, o.filled, o.limit);
}
void ChainDB::change_fillstate(const chain_db::OrderFillstate& o, bool buy)
{
    if (buy)
        stmtUpdateFillQuoteBuyOrder.run(o.filled, o.id);
    else
        stmtUpdateFillBaseSellOrder.run(o.filled, o.id);
}

void ChainDB::delete_order(const chain_db::OrderDelete& od)
{
    if (od.buy)
        stmtDeleteQuoteBuyOrder.run(od.id);
    else
        stmtDeleteBaseSellOrder.run(od.id);
}

std::optional<chain_db::OrderData> ChainDB::select_order(TransactionId id) const
{
    using ret_t = chain_db::OrderData;

    std::optional<chain_db::OrderData> res {
        stmtSelectBaseSell.one(id.accountId, id.pinHeight, id.nonceId).process([&](const sqlite::Row& o) {
            return ret_t {
                .id = o[0],
                .buy = false,
                .txid { id },
                .aid = o[1],
                .total = o[2],
                .filled = o[3],
                .limit = o[4]
            };
        })
    };
    if (!res) {
        res = stmtSelectQuoteBuy.one(id.accountId, id.pinHeight, id.nonceId).process([&](auto o) {
            return ret_t {
                .id = o[0],
                .buy = true,
                .txid = id,
                .aid = o[1],
                .total = o[2],
                .filled = o[3],
                .limit = o[4]
            };
        });
    }
    return res;
}
OrderLoaderAscending ChainDB::base_order_loader_ascending(AssetId aid) const
{
    return { stmtSelectBaseSellOrderAsc.bind_multiple(aid) };
}

OrderLoaderDescending ChainDB::quote_order_loader_descending(AssetId aid) const
{
    return { stmtSelectQuoteBuyOrderDesc.bind_multiple(aid) };
}

void ChainDB::insert_canceled(CancelId cid, AccountId aid, PinHeight ph, NonceId nid)
{
    stmtInsertCanceled.run(cid, aid, ph, nid);
}
void ChainDB::delete_canceled(CancelId cid)
{
    stmtDeleteCanceled.run(cid);
}

void ChainDB::insert_pool(const PoolData& d)
{
    stmtInsertPool.run(d.asset_id(), d.quote, d.base, d.shares_total());
}

std::optional<PoolData> ChainDB::select_pool(AssetId assetId) const
{
    return stmtSelectPool.one(assetId).process([](auto o) -> PoolData {
        return PoolData {
            { .assetId = o[0],
                .base = o[1],
                .quote = o[2],
                .shares = o[3] }
        };
    });
}

void ChainDB::update_pool(TokenId shareId, Funds_uint64 base, Funds_uint64 quote, Funds_uint64 shares)
{
    stmtUpdatePool.run(base, quote, shares, shareId);
}

void ChainDB::set_pool_liquidity(AssetId assetId, const defi::PoolLiquidity_uint64& pl)
{
    stmtUpdatePoolLiquidity.run(pl.base, pl.quote, assetId);
}
void ChainDB::insert_token_fork_balance(TokenForkBalanceId id, TokenId tokenId, TokenForkId forkId, Funds_uint64 balance)
{
    stmtTokenForkBalanceInsert.run(id, tokenId, forkId, balance);
}

// bool ChainDB::fork_balance_exists(AccountToken at, NonzeroHeight h)
// {
//     return stmtTokenForkBalanceEntryExists.one(at.account_id(), at.asset_id(), h)
//         .process([](auto& o) {
//             return o.has_value();
//         });
// }

std::optional<std::pair<NonzeroHeight, Funds_uint64>> ChainDB::get_balance_snapshot_after(TokenId tokenId, NonzeroHeight minHegiht) const
{
    auto res { stmtTokenForkBalanceSelect.one(tokenId, minHegiht) };
    if (!res.has_value())
        return {};
    return std::pair<NonzeroHeight, Funds_uint64> { res[0], res[1] };
}

void ChainDB::insert_new_token(const AssetData& d)
{
    auto id { cache.nextAssetId++ };
    if (id != d.id)
        throw std::runtime_error("Internal error, token id inconsistent.");
    // , stmtAssetInsert(db, "INSERT INTO `Assets` ( `id`, `hash, `name`, `precision`, `height`, `owner_account_id`, total_supply, group_id, parent_id, data) VALUES (?,?,?,?,?,?,?,?,?,?)")
    stmtAssetInsert.run(d.id, d.hash, d.name, d.supply.precision.value(), d.height, d.ownerAccountId, d.supply.funds.value(), d.groupId, d.parentId, d.data);
}

std::optional<NonzeroHeight> ChainDB::get_latest_fork_height(TokenId tid, Height h)
{
    auto res { stmtTokenSelectForkHeight.one(tid, h) };
    if (!res.has_value())
        return {};
    return NonzeroHeight { res[0] };
}

void ChainDB::insert_token_balance(AccountToken at, Funds_uint64 balance)
{
    stmtTokenInsertBalance.run(cache.nextStateId, at.token_id(), at.account_id(), balance);
    cache.nextStateId++;
}

std::optional<std::pair<BalanceId, Funds_uint64>> ChainDB::get_balance(AccountId aid, TokenId tid) const
{
    auto res { stmtTokenSelectBalance.one(tid, aid) };
    if (!res.has_value())
        return {};
    return std::pair { res.get<BalanceId>(0), res.get<Funds_uint64>(1) };
}

std::vector<std::pair<TokenId, Funds_uint64>> ChainDB::get_tokens(AccountId accountId, size_t limit)
{
    return stmtAccountSelectAssets.all([&](const sqlite::Row& r) {
        return std::pair { TokenId { r[0] }, Funds_uint64 { r[1] } };
    },
        accountId, limit);
}

void ChainDB::set_balance(BalanceId id, Funds_uint64 balance)
{
    stmtTokenUpdateBalance.run(balance, id);
}

api::Richlist ChainDB::lookup_richlist(TokenId tokenId, size_t limit) const
{
    api::Richlist out;
    stmtTokenSelectRichlist.for_each([&](sqlite::Row& r) {
        out.entries.push_back({ r[0], r[1] });
    },
        tokenId, limit);
    return out;
}

std::tuple<std::vector<Batch>, HistoryHeights, AccountHeights> ChainDB::getConsensusHeaders() const
{
    uint32_t h = 1;
    std::vector<Batch> batches;
    HistoryHeights historyHeights;
    AccountHeights accountHeights;
    Batch b;
    stmtConsensusHeaders.for_each([&](sqlite::Row& r) {
        if (h != r.get<Height>(0)) { // corrupted
            throw std::runtime_error("Database corrupted, block height not consecutive");
        }
        historyHeights.append(r.get<HistoryId>(1));
        accountHeights.append(r.get<AccountId>(2));
        Header header { r.get_array<80>(3) };
        if (b.size() >= HEADERBATCHSIZE) {
            assert(b.complete());
            batches.push_back(std::move(b));
            b.clear();
        }
        b.append(header);
        h += 1;
    });
    if (b.size() > 0) {
        batches.push_back(std::move(b));
    }
    return { std::move(batches), std::move(historyHeights), std::move(accountHeights) };
}

void ChainDB::insert_bad_block(NonzeroHeight height,
    const HeaderView header)
{
    stmtBadblockInsert.run(height, header);
}

std::vector<std::pair<Height, Header>>
ChainDB::getBadblocks() const
{
    return stmtBadblockGet.all([&](const sqlite::Row& r) {
        return std::pair<Height, Header> { r[0], r[1] };
    });
}

void ChainDB::insert_history_link(HistoryId parent, HistoryId link)
{
    stmtHistoryLinkInsert.run(parent, link);
}

HistoryId ChainDB::insertHistory(const HashView hash,
    const std::vector<uint8_t>& data)
{
    stmtHistoryInsert.run((int64_t)cache.nextHistoryId.value(), hash, data);
    return cache.nextHistoryId++;
}

void ChainDB::delete_history_from(NonzeroHeight h)
{
    const int64_t nextHistoryId = stmtConsensusSelectHistory.one(h).get<int64_t>(0);
    assert(nextHistoryId >= 0);
    stmtHistoryDeleteFrom.run(nextHistoryId);
    stmtAccountHistoryDeleteFrom.run(nextHistoryId);
    cache.nextHistoryId = HistoryId { nextHistoryId };
}

std::optional<std::pair<history::HistoryVariant, HistoryId>> ChainDB::lookup_history(const HashView hash) const
{
    return stmtHistoryLookup.one(hash).process([](auto& o) {
        return std::pair<history::HistoryVariant, HistoryId> {
            std::vector<uint8_t> { o[1] }, o[0]
        };
    });
}

size_t ChainDB::byte_size() const
{
    return stmtGetDBSize.one().get<int64_t>(0);
}

std::vector<std::pair<HistoryId, history::Entry>> ChainDB::lookup_history_range(HistoryId lower, HistoryId upper) const
{
    int64_t l = lower.value();
    int64_t u = (upper == HistoryId { 0 } ? std::numeric_limits<int64_t>::max() : upper.value());
    try {
        return stmtHistoryLookupRange.all([&](const sqlite::Row& r) {
            return std::pair<HistoryId, history::Entry> { r[0], { r[1], std::vector<uint8_t> { r[2] } } };
        },
            l, u);
    } catch (...) {
        spdlog::error("Cannot load history entries [{},{}]", lower.value(), upper.value());
        throw;
    }
}

std::optional<history::Entry> ChainDB::lookup_history(HistoryId id) const
{
    auto v { lookup_history_range(id, id + 1) };
    if (v.size() == 0)
        return std::nullopt;
    return std::move(v.front().second);
}

history::Entry ChainDB::fetch_history(HistoryId id) const
{
    if (auto e { lookup_history(id) })
        return std::move(*e);
    throw std::runtime_error("Cannot load database entry with id " + std::to_string(id.value()));
}

void ChainDB::insertAccountHistory(AccountId accountId, HistoryId historyId)
{
    stmtAccountHistoryInsert.run(accountId, historyId);
}

std::optional<AccountId> ChainDB::lookup_account(const AddressView address) const
{
    return stmtAddressLookup.one(address).process([](auto& p) {
        return AccountId { p[0] };
    });
}

std::vector<std::tuple<HistoryId, history::Entry>> ChainDB::lookup_history_100_desc(AccountId accountId, int64_t beforeId)
{
    return stmtHistoryById.all(
        [&](const sqlite::Row& row) {
            return std::tuple<HistoryId, history::Entry>(
                { row[0], { row[1], std::vector<uint8_t>(row[2]) } });
        },
        accountId, beforeId);
}

std::optional<Address> ChainDB::lookup_address(AccountId id) const
{
    return stmtAccountsLookup.one(id).process([](auto& o) {
        return Address { o[0] };
    });
}

Address ChainDB::fetch_address(AccountId id) const
{
    auto p = lookup_address(id);
    if (!p) {
        throw std::runtime_error("Database corrupted (fetch_address(" + std::to_string(id.value()) + ")");
    }
    return *p;
}

auto ChainDB::get_token_balance(BalanceId id) const -> std::optional<Balance>
{

    return stmtSelectBalanceId.one(id).process([&](const sqlite::Row& o) {
        return Balance {
            .balanceId = id,
            .accountId = o[0],
            .tokenId = o[1],
            .balance = o[2]
        };
    });
}

std::optional<AssetDetail> ChainDB::lookup_asset(AssetId id) const
{
    // , stmtAssetLookup(db, "SELECT (id, hash, name, precision, height, owner_account_id, total_supply, group_id, parent_id) FROM Assets WHERE `id`=?")
    return stmtAssetLookup.one(id).process([](auto& o) -> AssetDetail {
        return {
            AssetBasic {
                .id = o[0],
                .hash = o[1],
                .name = o[2],
                .precision = o[3],
            },
            {
                .height = o[4],
                .ownerAccountId = o[5],
                .totalSupply = o[6],
                .group_id = o[7],
                .parent_id = o[8],
            }
        };
    });
}

AssetDetail ChainDB::fetch_asset(AssetId id) const
{
    auto p { lookup_asset(id) };
    if (!p) {
        throw std::runtime_error("Database corrupted (fetch_token(" + std::to_string(id.value()) + ")");
    }
    return *p;
}
std::optional<AssetDetail> ChainDB::lookup_asset(const AssetHash& hash) const
{
    return stmtTokenLookupByHash.one(hash).process([](auto& o) -> AssetDetail {
        return {
            AssetBasic {
                .id = o[0],
                .hash = o[1],
                .name = o[2],
                .precision = o[3],
            },
            {
                .height = o[4],
                .ownerAccountId = o[5],
                .totalSupply = o[6],
                .group_id = o[7],
                .parent_id = o[8],
            }
        };
    });
}

AssetDetail ChainDB::fetch_asset(const AssetHash& hash) const
{
    auto p { lookup_asset(hash) };
    if (!p) {
        throw std::runtime_error("Database corrupted (fetch_token(" + hash.hex_string() + ")");
    }
    return *p;
}

std::optional<BlockId> ChainDB::lookup_block_id(const HashView hash) const
{
    return stmtBlockIdSelect.one(hash);
}

std::optional<NonzeroHeight> ChainDB::lookup_block_height(const HashView hash) const
{
    return stmtBlockHeightSelect.one(hash).process([](auto& r) {
        return NonzeroHeight { r[0] };
    });
}

void ChainDB::delete_bad_block(HashView blockhash)
{
    auto o = stmtBlockIdSelect.one(blockhash);
    if (!o.has_value()) {
        spdlog::error("Database error: Cannot delete bad block with hash {}",
            serialize_hex(blockhash));
        return;
    }
    BlockId id { o[0] };
    stmtBlockDelete.run(id);
    stmtScheduleDelete.run(id);
}

std::pair<std::optional<BalanceId>, Funds_uint64> ChainDB::get_token_balance_recursive(AccountId aid, TokenId tid, api::AssetLookupTrace* trace) const
{
    while (true) {
        // direct lookup
        if (auto b { get_balance(aid, tid) })
            return *b;

        auto assetId { tid.asset_id() };
        if (!assetId /*i.e. tid == TokenId::WART*/ || tid.is_share())
            goto notfound; // WART and pool share token types cannot have any parent

        // get token fork height
        auto a { lookup_asset(*assetId) };
        if (!a)
            throw std::runtime_error("Database error: Cannot find token info for id " + std::to_string(tid.value()) + ".");
        auto h { a->height };
        auto& pid { a->parent_id };
        if (!pid) // has no parent, i.e. was not forked, no entry found
            goto notfound;
        if (trace)
            trace->fails.push_back(*a);
        if (auto o { get_balance_snapshot_after(*pid, h) }) {
            auto& [height, funds] { *o };
            if (trace)
                trace->snapshotHeight = height;
            return { std::nullopt, funds };
        };
        tid = *pid;
    }
notfound:
    return { std::nullopt, Funds_uint64::zero() };
}

std::pair<std::optional<BalanceId>, Wart> ChainDB::get_wart_balance(AccountId aid) const
{
    auto [id, bal] { get_token_balance_recursive(aid, TokenId::WART, nullptr) };
    return { id, Wart::from_funds_throw(bal) };
}

chainserver::TransactionIds ChainDB::fetch_tx_ids(Height height) const
{
    const auto r = chainserver::TransactionIds::block_range(height);
    chainserver::TransactionIds out;
    spdlog::debug("Loading nonces from blocks {} to {} into cache...", r.hbegin.value(), r.hend.value());
    auto ids { consensus_block_ids(r) };
    for (size_t i = 0; i < r.length(); ++i) {
        if ((i & 15) == 0 && shutdownSignal) {
            throw std::runtime_error("Shutdown initiated");
        }
        Height height = r.hbegin + i;
        auto id = ids[i];
        auto b { get_block(id) };
        if (!b) {
            throw std::runtime_error("Database corrupted (consensus block id " + std::to_string(id.value()) + "+ not available)");
        }
        assert(height == b->height);
        for (auto& tid : b->tx_ids()) {
            if (out.emplace(tid).second == false) {
                throw std::runtime_error(
                    "Database corrupted (duplicate transaction id in chain)");
            };
        }
    }
    return out;
}
}
