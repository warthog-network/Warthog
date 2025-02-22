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
#include <array>
#include <spdlog/spdlog.h>

ChainDB::Cache ChainDB::Cache::init(SQLite::Database& db)
{
    auto maxaAccountId = AccountId(int64_t(db.execAndGet("SELECT coalesce(max(id),0) FROM `Accounts`")
            .getInt64()));

    auto maxTokenId_i64 { int64_t(db.execAndGet("SELECT coalesce(max(id),0) FROM `Tokens`").getInt64()) };
    assert(maxTokenId_i64 >= 0 && maxTokenId_i64 < std::numeric_limits<uint32_t>::max());
    auto maxTokenId { TokenId(maxTokenId_i64) };

    auto maxBalanceId_i64 { int64_t(db.execAndGet("SELECT coalesce(max(ROWID)+1,1) FROM `Balance`").getInt64()) };
    assert(maxTokenId_i64 >= 0 && maxTokenId_i64 < std::numeric_limits<uint32_t>::max());
    auto maxBalanceId { BalanceId(maxBalanceId_i64) };

    int64_t hid = db.execAndGet("SELECT coalesce(max(id)+1,1) FROM History")
                      .getInt64();
    if (hid < 0)
        throw std::runtime_error("Database corrupted, negative history id.");
    return {
        .maxAccountId { maxaAccountId },
        .maxTokenId { maxTokenId },
        .nextHistoryId = HistoryId { uint64_t(hid) },
        .nextBalanceId { maxBalanceId },
        .deletionKey { 2 }
    };
}

ChainDBTransaction ChainDB::transaction()
{
    return ChainDBTransaction(*this);
}
ChainDB::ChainDB(const std::string& path)
    : db(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)
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
    , stmtTokenInsert(db, "INSERT INTO \"Tokens\" ( `id`, `height`, `creator_id`, `name`, `hash`, `type`) VALUES (?,?,?,?,?,?)")
    , stmtTokenLookup(db, "SELECT (name, hash) FROM `Tokens` WHERE `id`=?")
    , stmtSelectBalanceId(db, "SELECT `account_id`, `token_id`, `balance` where `id`=?")
    , stmtTokenInsertBalance(db, "INSERT INTO \"Balance\" ( `id`, `token_id`, `account_id`, `balance`) VALUES (?,?,?)")
    , stmtTokenSelectBalance(db, "SELECT `id`, `balance` FROM `Balance` WHERE `token_id`=? AND `account_id`=?")
    , stmtAccountSelectTokens(db, "SELECT `token_id`, `balance` FROM `Balance` WHERE `account_id`=? LIMIT ?")
    , stmtTokenUpdateBalance(db, "UPDATE `Balance` SET `balance`=? WHERE `token_id`=? AND `account_id`=?")
    , stmtTokenSelectRichlist(db, "SELECT `account_id`, `balance` FROM `Balance` WHERE `token_id`=? ORDER BY `balance` DESC LIMIT ?")
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

    , stmtAccountInsert(db, "INSERT INTO `Accounts` ( `id`, `address`"
                            ") VALUES (?,?)")
    , stmtAccountDeleteFrom(db, "DELETE FROM `Accounts` WHERE `id`>=?")

    , stmtBadblockInsert(
          db, "INSERT INTO `Badblocks` (`height`, `header`) VALUES (?,?)")
    , stmtBadblockGet(db, "SELECT `height`, `header` FROM `Badblocks`")
    , stmtAccountLookup(
          db, "SELECT `Address`, `Balance` FROM `Accounts` WHERE id=?")
    , stmtRichlistLookup(
          db, "SELECT Address, Balance FROM `Accounts` ORDER BY `Balance` DESC LIMIT ?")
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
    , stmtInsertForkEvent(db, "INSERT INTO \"ForkEvents\" (`id`, `height`, `totalTokens`) VALUES (?,?,?)")
    , stmtDeleteForkEvents(db, "DELETE FROM \"ForkEvents\" WHERE `height`>=?")
{

    //
    // Do DELETESCHEDULE cleanup
    db.exec("UPDATE `Deleteschedule` SET `deletion_key`=1");
}

void ChainDB::insert_account(const AddressView address, AccountId verifyNextAccountId)
{
    if (cache.maxAccountId + 1 != verifyNextAccountId)
        throw std::runtime_error("Internal error, state id inconsistent.");
    stmtAccountInsert.run(cache.maxAccountId + 1, address);
    cache.maxAccountId++;
}

void ChainDB::delete_state_from(AccountId fromAccountId)
{
    assert(fromAccountId.value() > 0);
    if (cache.maxAccountId + 1 < fromAccountId) {
        spdlog::error("BUG: Deleting nothing, fromAccountId = {} > {} = cache.maxAccountId", fromAccountId.value(), cache.maxAccountId.value());
    } else {
        cache.maxAccountId = fromAccountId - 1;
        stmtAccountDeleteFrom.run(fromAccountId);
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
        throw std::runtime_error(fmt::format("Database corrupted. Signed snapshot invalid: {}", e.strerror()));
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
    stmtConsensusSelectRange.for_each([&](Statement2::Row& r) {
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
        return Block {
            .height = o[0],
            .header = o[1],
            .body = o[2]
        }
            .parse_throw();
    } catch (...) {
        throw std::runtime_error("Cannot load block with id " + std::to_string(id.value()) + ". ");
    }
}

std::optional<std::pair<BlockId, Block>> ChainDB::get_block(HashView hash) const
{
    auto o = stmtBlockByHash.one(hash);
    if (!o.has_value())
        return {};
    return std::pair<BlockId, Block> {
        o[0],
        Block {
            .height = o[1],
            .header = o[2],
            .body = o[3] }
    };
}

std::pair<BlockId, bool> ChainDB::insert_protect(const ParsedBlock& b)
{
    auto hash { b.header().hash() };

    auto blockId { lookup_block_id(hash) };
    if (blockId.has_value()) {
        assert(schedule_exists(*blockId) || consensus_exists(b.height(), *blockId));
        return { blockId.value(), false };
    } else {
        stmtBlockInsert.run(b.height(), b.header(), b.body().data(), hash);
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

void ChainDB::insert_consensus(NonzeroHeight height, BlockId blockId, HistoryId historyCursor, AccountId accountCursor)
{
    stmtConsensusInsert.run(height, blockId, historyCursor, accountCursor);
    stmtScheduleDelete2.run(blockId);
}

void ChainDB::insert_new_token(CreatorToken ct, NonzeroHeight height, TokenName name, TokenHash hash, TokenMintType type)
{
    auto id { cache.maxTokenId + 1 };
    if (id != ct.token_id())
        throw std::runtime_error("Internal error, token id inconsistent.");
    std::string n { name.c_str() };
    stmtTokenInsert.run(id, height, ct.creator_id(), n, hash, static_cast<uint8_t>(type));
    cache.maxTokenId++;
}

BalanceId ChainDB::insert_token_balance(TokenId tokenId, AccountId accountId, Funds balance)
{
    stmtTokenInsertBalance.run(cache.nextBalanceId, tokenId, accountId, balance);
    return cache.nextBalanceId++;
}

std::optional<std::pair<BalanceId, Funds>> ChainDB::get_token_balance(TokenId tokenId, AccountId accountId)
{
    auto res { stmtTokenSelectBalance.one(tokenId, accountId) };
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

void ChainDB::set_balance(AccountToken at, Funds balance)
{
    stmtTokenUpdateBalance.run(balance, at.token_id(), at.account_id());
}

std::vector<std::pair<AccountId, Funds>> ChainDB::get_richlist(TokenId tokenId, size_t limit)
{
    std::vector<std::pair<AccountId, Funds>> res;
    stmtTokenSelectRichlist.for_each([&](Statement2::Row& r) {
        res.push_back({ r.get<AccountId>(0), r.get<Funds>(1) });
    },
        tokenId, limit);
    return res;
}

std::tuple<std::vector<Batch>, HistoryHeights, AccountHeights> ChainDB::getConsensusHeaders() const
{
    uint32_t h = 1;
    std::vector<Batch> batches;
    ;
    HistoryHeights historyHeights;
    AccountHeights accountHeights;
    Batch b;
    stmtConsensusHeaders.for_each([&](Statement2::Row& r) {
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
    stmtBadblockGet.for_each([&](Statement2::Row& r) {
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

std::vector<std::pair<Hash, std::vector<uint8_t>>> ChainDB::lookupHistoryRange(HistoryId lower, HistoryId upper)
{
    std::vector<std::pair<Hash, std::vector<uint8_t>>> out;
    int64_t l = lower.value();
    int64_t u = (upper == HistoryId { 0 } ? std::numeric_limits<int64_t>::max() : upper.value());
    stmtHistoryLookupRange.for_each([&](Statement2::Row& r) {
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
        [&](Statement2::Row& row) {
            out.push_back({ row[0], row[1], row[2] });
        },
        accountId, beforeId);
    return out;
}

void ChainDB::insert_fork_event(int64_t id, Height height, size_t totalTokens)
{
    stmtInsertForkEvent.run(id, height, totalTokens);
}

void ChainDB::delete_fork_events(Height fromHeight)
{
    stmtDeleteForkEvents.run(fromHeight);
}

std::optional<AddressFunds> ChainDB::lookup_account(AccountId id) const
{
    return stmtAccountLookup.one(id).process([](auto& o) {
        return AddressFunds { o[0], o[1] };
    });
}

AddressFunds ChainDB::fetch_account(AccountId id) const
{
    auto p = lookup_account(id);
    if (!p) {
        throw std::runtime_error("Database corrupted (fetch_account(" + std::to_string(id.value()) + ")");
    }
    return *p;
}

auto ChainDB::lookup_balance(BalanceId id) const -> std::optional<Balance>
{
    return stmtSelectBalanceId.one(id).process([&](auto& o) {
        return Balance {
            .balanceId { id },
            .accountId { o[0] },
            .tokenId { o[1] },
            .balance { o[2] }
        };
    });
}

std::optional<TokenInfo> ChainDB::lookup_token(TokenId id) const
{
    return stmtTokenLookup.one(id).process([](auto& o) -> TokenInfo {
        return {
            .name { o[0] },
            .hash { o[1] }
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

api::Richlist ChainDB::lookup_richlist(uint32_t N) const
{
    api::Richlist out;
    stmtRichlistLookup.for_each([&](Statement2::Row& r) {
        out.entries.push_back({ r[0], r[1] });
    },
        N);
    return out;
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
