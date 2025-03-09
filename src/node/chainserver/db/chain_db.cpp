#include "chain_db.hpp"
#include "api/types/all.hpp"
#include "block/body/parse.hpp"
#include "block/chain/header_chain.hpp"
#include "block/header/header_impl.hpp"
#include "block/header/view_inline.hpp"
#include "defi/token/account_token.hpp"
#include "defi/token/info.hpp"
#include "defi/token/token.hpp"
#include "general/hex.hpp"
#include "general/now.hpp"
#include "general/writer.hpp"
#include "global/globals.hpp"
#include "sqlite3.h"
#include <spdlog/spdlog.h>

namespace {
enum METATYPES { MAXSTATE = 0 };
}

using namespace std::string_literals;
ChainDB::Cache ChainDB::Cache::init(SQLite::Database& db)
{
    auto get_uint64 {
        [&db](const std::string& s) { return int64_t(db.execAndGet(s).getInt64()); }
    };

    auto nextStateId = get_uint64("SELECT COALESCE(0,value)+1 FROM Metadata WHERE key=" + std::to_string(METATYPES::MAXSTATE));

    auto nextAccountId = AccountId(get_uint64("SELECT coalesce(max(ROWID),0) FROM `Accounts`"));
    auto nextTokenId = TokenId(get_uint64("SELECT coalesce(max(ROWID),0) FROM `Tokens`"));

    int64_t hid = db.execAndGet("SELECT coalesce(max(id)+1,1) FROM History")
                      .getInt64();
    if (hid < 0)
        throw std::runtime_error("Database corrupted, negative history id.");
    return {
        .nextAccountId{nextAccountId},
        .nextTokenId{nextTokenId},
        .nextStateId =nextStateId,
        .nextHistoryId = HistoryId { uint64_t(hid) },
        .deletionKey { 2 }

    };
}

ChainDBTransaction ChainDB::transaction()
{
    return ChainDBTransaction(*this);
}
ChainDB::ChainDB(const std::string& path)
    : db([&]() -> auto& {
    spdlog::debug("Opening chain database \"{}\"", path);
    return path; }(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)
    , fl(path)
    , createTables(db)
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
    , stmtSelectCandles5m(db, "SELECT (timestamp, open, high, low, close, quantity, volume) FROM Candles5m WHERE token_id=? AND timestamp>=? AND timestamp<=?")
    , stmtPruneCandles1h(db, "DELETE FROM Candles1h WHERE timestamp >=?")
    , stmtInsertCandles1h(db, "INSERT OR REPLACE INTO Candles1h (tokenId, timestamp, open, high, low, close, quantity, volume) VALUES (?,?,?,?,?,?,?,?)")
    , stmtSelectCandles1h(db, "SELECT (timestamp, open, high, low, close, quantity, volume) FROM Candles1h WHERE token_id=? AND timestamp>=? AND timestamp<=?")

    , stmtInsertBaseSellOrder(db, "INSERT INTO SellOrders (id, account_id,token_id, totalBase, filledBase, price) VALUES(?,?,?,?,?,?)")
    , stmtInsertQuoteBuyOrder(db, "INSERT INTO BuyOrders (id, account_id,token_id, totalQuote, filledQuote, price) VALUES(?,?,?,?,?,?)")
    , stmtSelectBaseSellOrderAsc(db, "SELECT (id, account_id, totalBase, filledBase, price) FROM SellOrders WHERE token_id=? ORDER BY price ASC, id ASC")
    , stmtSelectQuoteBuyOrderDesc(db, "SELECT (id, account_id, totalQuote, filledQuote, price) FROM BuyOrders WHERE token_id=? ORDER BY price DESC, id ASC")
    , stmtInsertPool(db, "INSERT INTO Pools (id, token_id, pool_wart, pool_token, pool_shares) VALUES (?,?,0,0,0)")
    , stmtSelectPool(db, "SELECT (id, token_id, liquidity_token, liquidity_wart, pool_shares) FROM Pools WHERE token_id=? OR id=?")
    , stmtUpdatePool(db, "Update Pools SET liquidity_base=?, liquidity_quote=?, pool_shares=? WHERE id=?")
    , stmtTokenForkBalanceInsert(db, "INSERT INTO TokenForkBalances "
                                     "(id, account_id, token_id, height, balance) "
                                     "VALUES (?,?,?,?)")
    , stmtTokenForkBalanceEntryExists(db, "SELECT 1 FROM TokenForkBalances "
                                          "WHERE account_id=? "
                                          "AND token_id=? "
                                          "AND height=?")
    , stmtTokenForkBalanceSelect(db, "SELECT height, balance FROM `TokenForkBalances` WHERE token_id=? height>=? ORDER BY height ASC LIMIT 1")

    , stmtTokenForkBalancePrune(db, "DELETE FROM TokenForkBalances WHERE id>=?")

    , stmtTokenInsert(db, "INSERT INTO `Tokens` ( `id`, `height`, `owner_account_id`, total_supply, group_id, parent_id, name, hash, data) VALUES (?,?,?,?,?,?,?)")
    , stmtTokenPrune(db, "DELETE FROM Tokens WHERE id>=?")
    , stmtTokenMaxSnapshotHeight(db, "SELECT height FROM Tokens "
                                     "WHERE parent_id=? AND height>=? "
                                     "ORDER BY height DESC LIMIT 1")
    , stmtTokenSelectForkHeight(db, "SELECT height FROM Tokens WHERE parent_id=? AND height>=? ORDER BY height DESC LIMIT 1")
    , stmtTokenLookup(db, "SELECT (height, owner_account_id, total_supply, group_id, parent_id, name, hash, data) FROM Tokens WHERE `id`=?")
    , stmtSelectBalanceId(db, "SELECT `account_id`, `token_id`, `balance` FROM `Balances` WHERE `id`=?")
    , stmtTokenInsertBalance(db, "INSERT INTO `Balances` ( id, `token_id`, `account_id`, `balance`) VALUES (?,?,?,?)")
    , stmtBalancePrune(db, "DELETE FROM Balances WHERE id>=?")
    , stmtTokenSelectBalance(db, "SELECT `id`, `balance` FROM `Balances` WHERE `token_id`=? AND `account_id`=?")
    , stmtAccountSelectTokens(db, "SELECT `token_id`, `balance` FROM `Balances` WHERE `account_id`=? LIMIT ?")
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
    , stmtScheduleDelete2(db, "DELETE FROM `Deleteschedule` WHERE `block_id` = ?")
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
    , stmtHistoryInsert(db, "INSERT INTO `History` (`id`,`hash`, `data`"
                            ") VALUES (?,?,?)")
    , stmtHistoryDeleteFrom(db, "DELETE FROM `History` WHERE `id`>=?")
    , stmtHistoryLookup(db,
          "SELECT `id`, `data` FROM `History` WHERE `hash`=?")
    , stmtHistoryLookupRange(db,
          "SELECT `hash`, `data` FROM `History` WHERE `id`>=? AND`id`<?")
    , stmtAccountHistoryInsert(db, "INSERT INTO `AccountHistory` "
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
    , stmtAddressLookup(
          db, "SELECT `ROWID`,`balance` FROM `Accounts` WHERE `address`=?")
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
    if (cache.nextStateId != verifyNextAccountId.value())
        throw std::runtime_error("Internal error, state id inconsistent.");
    stmtAccountsInsert.run(cache.nextStateId, address);
    cache.nextStateId++;
}

void ChainDB::delete_state_from(uint64_t fromStateId)
{
    assert(fromStateId > 0);
    if (cache.nextStateId <= fromStateId) {
        spdlog::error("BUG: Deleting nothing, fromAccountId = {} >= {} = cache.maxAccountId", fromStateId, cache.nextStateId);
    } else {
        cache.nextStateId = fromStateId;
        stmtAccountsDeleteFrom.run(fromStateId);
        stmtTokenForkBalancePrune.run(fromStateId);
        stmtTokenPrune.run(fromStateId);
        stmtBalancePrune.run(fromStateId);
    }
}

Worksum ChainDB::get_consensus_work() const
{
    auto o { stmtConsensusSelect.one(WORKSUMID) };
    if (!o.has_value()) {
        throw std::runtime_error("Database corrupted. No worksum entry");
    }
    return o.get_array<32>(0);
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
    std::vector<BlockId> out;
    stmtConsensusSelectRange.for_each([&](sqlite::Row& r) {
        out.push_back(r[0]);
    },
        range.hbegin, range.hend);
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
    assert(stmtScheduleBlock.run(0, id.value()) == 1);
}

DeletionKey ChainDB::delete_consensus_from(NonzeroHeight height)
{
    auto dk { cache.deletionKey++ };
    stmtScheduleConsensus.run(dk.value(), height);
    stmtConsensusDeleteFrom.run(height);
    return dk;
}

std::optional<ParsedBlock> ChainDB::get_block(BlockId id) const
{
    auto o { stmtBlockById.one(id) };
    if (!o.has_value())
        return {};
    try {
        return ParsedBlock::create_throw(o[0], Header(o[1]), o[2]);
    } catch (...) {
        throw std::runtime_error("Cannot load block with id " + std::to_string(id.value()) + ". ");
    }
}

std::optional<std::pair<BlockId, ParsedBlock>> ChainDB::get_block(HashView hash) const
{
    auto o = stmtBlockByHash.one(hash);
    if (!o.has_value())
        return {};
    try {
        return std::pair<BlockId, ParsedBlock> {
            o[0],
            ParsedBlock::create_throw(o[1], Header(o[2]), o[3])
        };
    } catch (...) {
        throw std::runtime_error("Cannot load block with hash " + serialize_hex(hash) + ".");
    }
}

std::pair<BlockId, bool> ChainDB::insert_protect(const ParsedBlock& b)
{
    auto hash { b.header.hash() };

    auto blockId { lookup_block_id(hash) };
    if (blockId.has_value()) {
        assert(schedule_exists(*blockId) || consensus_exists(b.height, *blockId));
        return { blockId.value(), false };
    } else {
        stmtBlockInsert.run(b.height, b.header, b.body.data(), hash);
        auto lastId { db.getLastInsertRowid() };
        stmtScheduleInsert.run(lastId, 0);
        return { BlockId(lastId), true };
    }
}

std::optional<BlockUndoData>
ChainDB::get_block_undo(BlockId id) const
{
    return stmtBlockGetUndo.one(id).process([](auto& a) {
        return BlockUndoData {
            .header { a[0] },
            .rawBody { a.get_vector(1) },
            .rawUndo { a.get_vector(2) }
        };
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
    stmtSelectCandles5m.one(tid, ts, ts).process([](auto& o) {
        return Candle {
            .timestamp { o[0] },
            .open { o[1] },
            .high { o[2] },
            .low { o[3] },
            .close { o[4] },
            .quantity { o[5] },
            .volume { o[6] },
        };
    });
}

std::vector<Candle> ChainDB::select_candles_5m(TokenId tid, Timestamp from, Timestamp to)
{
    stmtSelectCandles5m.for_each(tid, from, to, [](auto& o) {
        return Candle {
            .timestamp { o[0] },
            .open { o[1] },
            .high { o[2] },
            .low { o[3] },
            .close { o[4] },
            .quantity { o[5] },
            .volume { o[6] },
        };
    });
}

void ChainDB::insert_candles_1h(TokenId tid, const Candle& c)
{
    stmtInsertCandles1h.run(tid, c.timestamp, c.open, c.high, c.low, c.close, c.quantity, c.volume);
}

std::optional<Candle> ChainDB::select_candle_1h(TokenId tid, Timestamp ts)
{
    stmtSelectCandles1h.one(tid, ts, ts).process([](auto& o) {
        return Candle {
            .timestamp { o[0] },
            .open { o[1] },
            .high { o[2] },
            .low { o[3] },
            .close { o[4] },
            .quantity { o[5] },
            .volume { o[6] },
        };
    });
}

std::vector<Candle> ChainDB::select_candles_1h(TokenId tid, Timestamp from, Timestamp to)
{
    stmtSelectCandles1h.for_each(tid, from, to, [](auto& o) {
        return Candle {
            .timestamp { o[0] },
            .open { o[1] },
            .high { o[2] },
            .low { o[3] },
            .close { o[4] },
            .quantity { o[5] },
            .volume { o[6] },
        };
    });
}

void ChainDB::insert_buy_order(OrderId oid, AccountId aid, TokenId tid, Funds totalBase, Funds filledBase, Price_uint64 price)
{
    stmtInsertBaseSellOrder.run(oid, aid, tid, totalBase, filledBase, price);
}
void ChainDB::insert_quote_order(OrderId oid, AccountId aid, TokenId tid, Funds totalBase, Funds filledBase, Price_uint64 price)
{
    stmtInsertQuoteBuyOrder.run(oid, aid, tid, totalBase, filledBase, price);
}

OrderLoader ChainDB::base_order_loader(TokenId tid) const
{
    return { stmtSelectBaseSellOrderAsc.bind_multiple(tid) };
}

OrderLoader ChainDB::quote_order_loader(TokenId tid) const
{
    return { stmtSelectQuoteBuyOrderDesc.bind_multiple(tid) };
}

void ChainDB::insert_consensus(NonzeroHeight height, BlockId blockId, HistoryId historyCursor, uint64_t stateId)
{
    stmtConsensusInsert.run(height, blockId, historyCursor, stateId);
    stmtScheduleDelete2.run(blockId);
}

void ChainDB::insert_pool(TokenId shareId, TokenId tokenId)
{
    stmtInsertPool.run(shareId, tokenId);
}
std::optional<PoolData> ChainDB::select_pool(TokenId shareIdOrTokenId) const
{
    return stmtSelectPool.one(shareIdOrTokenId).process([](auto o) {
        return PoolData {
            .shareId { o[0] },
            .tokenId { o[1] },
            .base { o[2] },
            .quote { o[3] },
            .shares { o[4] }
        };
    });
}

void ChainDB::update_pool(TokenId shareId, Funds_uint64 base, Funds_uint64 quote, Funds_uint64 shares)
{
    stmtUpdatePool.run(base, quote, shares, shareId);
}

void ChainDB::insert_token_fork_balance(TokenForkBalanceId id, TokenId tokenId, TokenForkId forkId, Funds balance)
{
    stmtTokenForkBalanceInsert.run(id, tokenId, forkId, balance);
}

bool ChainDB::fork_balance_exists(AccountToken at, NonzeroHeight h)
{
    return stmtTokenForkBalanceEntryExists.one(at.account_id(), at.token_id(), h)
        .process([](auto& o) {
            return o.has_value();
        });
}

std::optional<std::pair<NonzeroHeight, Funds>> ChainDB::get_balance_snapshot_after(TokenId tokenId, NonzeroHeight minHegiht)
{
    auto res { stmtTokenForkBalanceSelect.one(tokenId, minHegiht) };
    if (!res.has_value())
        return {};
    return std::pair<NonzeroHeight, Funds> { res[0], res[1] };
}

void ChainDB::insert_new_token(CreatorToken ct, NonzeroHeight height, TokenName name, TokenHash hash, TokenMintType type)
{
    auto id { cache.nextStateId++ };
    if (id != ct.token_id().value())
        throw std::runtime_error("Internal error, token id inconsistent.");
    std::string n { name.c_str() };
    stmtTokenInsert.run(id, height, ct.creator_id(), n, hash, static_cast<uint8_t>(type));
}

std::optional<NonzeroHeight> ChainDB::get_latest_fork_height(TokenId tid, Height h)
{
    auto res { stmtTokenSelectForkHeight.one(tid, h) };
    if (!res.has_value())
        return {};
    return NonzeroHeight { res[0] };
}

void ChainDB::insert_token_balance(AccountToken at, Funds balance)
{
    stmtTokenInsertBalance.run(cache.nextStateId, at.token_id(), at.account_id(), balance);
    cache.nextStateId++;
}

std::optional<std::pair<BalanceId, Funds>> ChainDB::get_balance(AccountToken at) const
{
    auto res { stmtTokenSelectBalance.one(at.token_id(), at.account_id()) };
    if (!res.has_value())
        return {};
    return std::pair { res.get<BalanceId>(0), res.get<Funds>(1) };
}

std::vector<std::pair<TokenId, Funds>> ChainDB::get_tokens(AccountId accountId, size_t limit)
{
    std::vector<std::pair<TokenId, Funds>> res;
    stmtAccountSelectTokens.for_each([&](Statement2::Row& r) {
        res.push_back({ r.get<TokenId>(0), r.get<Funds>(1) });
    },
        accountId, limit);
    return res;
}

void ChainDB::set_balance(BalanceId id, Funds balance)
{
    stmtTokenUpdateBalance.run(balance, id);
}

api::Richlist ChainDB::lookup_richlist(TokenId tokenId, size_t limit) const
{
    api::Richlist out;
    stmtTokenSelectRichlist.for_each([&](Statement2::Row& r) {
        out.entries.push_back({ r[0], r[1] });
    },
        tokenId, limit);
    return out;
}

std::tuple<std::vector<Batch>, HistoryHeights, AccountHeights> ChainDB::getConsensusHeaders() const
{
    uint32_t h = 1;
    std::vector<Batch> batches;
    ;
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
    std::vector<std::pair<Height, Header>> res;
    stmtBadblockGet.for_each([&](sqlite::Row& r) {
        res.push_back({ r[0], r[1] });
    });
    return res;
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

std::optional<std::pair<std::vector<uint8_t>, HistoryId>> ChainDB::lookup_history(const HashView hash)
{
    return stmtHistoryLookup.one(hash).process([](auto& o) {
        return std::pair<std::vector<uint8_t>, HistoryId> {
            o[1],
            o[0]
        };
    });
}

size_t ChainDB::byte_size() const
{
    return stmtGetDBSize.one().get<int64_t>(0);
}

std::vector<std::pair<Hash, std::vector<uint8_t>>> ChainDB::lookupHistoryRange(HistoryId lower, HistoryId upper)
{
    std::vector<std::pair<Hash, std::vector<uint8_t>>> out;
    int64_t l = lower.value();
    int64_t u = (upper == HistoryId { 0 } ? std::numeric_limits<int64_t>::max() : upper.value());
    stmtHistoryLookupRange.for_each([&](sqlite::Row& r) {
        out.push_back({ r[0], r[1] });
    },
        l, u);
    return out;
}

void ChainDB::insertAccountHistory(AccountId accountId, HistoryId historyId)
{
    stmtAccountHistoryInsert.run(accountId, historyId);
}

std::optional<AccountFunds> ChainDB::lookup_address(const AddressView address) const
{
    return stmtAddressLookup.one(address).process([](auto& p) {
        return AccountFunds { p[0], p[1] };
    });
}

std::vector<std::tuple<HistoryId, Hash, std::vector<uint8_t>>> ChainDB::lookup_history_100_desc(
    AccountId accountId, int64_t beforeId)
{
    std::vector<std::tuple<HistoryId, Hash, std::vector<uint8_t>>> out;
    stmtHistoryById.for_each(
        [&](sqlite::Row& row) {
            out.push_back({ row[0], row[1], row[2] });
        },
        accountId, beforeId);
    return out;
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

// auto ChainDB::get_token_balance(BalanceId id) const -> std::optional<Balance>
// {
//     return stmtSelectBalanceId.one(id).process([&](auto& o) {
//         return Balance {
//             .balanceId { id },
//             .accountId { o[0] },
//             .tokenId { o[1] },
//             .balance { o[2] }
//         };
//     });
// }

std::optional<TokenInfo> ChainDB::lookup_token(TokenId id) const
{
    // , stmtTokenLookup(db, "SELECT (heightn,owner_account_id, total_supply, group_id, name, hash, data) FROM Tokens WHERE `id`=?")
    return stmtTokenLookup.one(id).process([&id](auto& o) -> TokenInfo {
        return {
            .id { id },
            .height { o[0] },
            .ownerAccountId { o[1] },
            .totalSupply { o[2] },
            .group_id { o[3] },
            .parent_id { o[4] },
            .name { o[5] },
            .hash { o[6] },
        };
    });
}

TokenInfo ChainDB::fetch_token(TokenId id) const
{
    auto p = lookup_token(id);
    if (!p) {
        throw std::runtime_error("Database corrupted (fetch_token(" + std::to_string(id.value()) + ")");
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
    stmtScheduleDelete2.run(id);
}

std::pair<std::optional<BalanceId>, Funds> ChainDB::get_token_balance_recursive(AccountToken ac, TokenLookupTrace* trace)
{
    while (true) {
        // direct lookup
        if (auto b { get_balance(ac) })
            return *b;

        // get token fork height
        auto o { lookup_token(ac.token_id()) };
        if (!o)
            throw std::runtime_error("Database error: Cannot find token info for id " + std::to_string(ac.token_id().value()) + ".");
        auto& tokenInfo { *o };
        auto h { tokenInfo.height };
        auto& p { tokenInfo.parent_id };
        if (!p) { // has no parent, i.e. was not forked, no entry found
            return { std::nullopt, Funds::zero() };
        }
        trace->steps.push_back({ *p, h });
        if (auto o { get_balance_snapshot_after(*p, h) }) {
            auto& [height, funds] { *o };
            trace->steps.back().snapshotHeight = height;
            return { std::nullopt, funds };
        };
        ac.token_id() = *p;
    }
}

bool ChainDB::write_snapshot_balance(AccountToken at, Funds f, NonzeroHeight tokenCreationHeight)
{
    // , stmtTokenForkBalanceInsert(db, "INSERT OR IGNORE INTO TokenForkBalances "
    //                                        "(id, token_id, height, balance) "
    //                                        "VALUES (?,?,?,?)")
    auto maxSnapshotHeight { stmtTokenMaxSnapshotHeight.one(at.token_id(), tokenCreationHeight).process([](auto& a) {
        return NonzeroHeight { a[0] };
    }) };
    if (!maxSnapshotHeight)
        return false;
    // auto res{stmtTokenForkBalanceInsert.run(cache.nextStateId, at.token_id(), tokenCreationHeight, f)};
    stmtTokenForkBalanceInsert.run(cache.nextStateId, at.token_id(), tokenCreationHeight, f);

    return o.get_array<32>(0);
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
        auto b = get_block(id);
        if (!b) {
            throw std::runtime_error("Database corrupted (consensus block id " + std::to_string(id.value()) + "+ not available)");
        }
        assert(height == b->height);
        assert(b->body.size() > 0);
        for (auto& tid : b->read_tx_ids()) {
            if (out.emplace(tid).second == false) {
                throw std::runtime_error(
                    "Database corrupted (duplicate transaction id in chain)");
            };
        }
    }
    return out;
}
