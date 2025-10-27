#include "consensus.hpp"
#include "cache.hpp"
#include "chainserver/db/chain_db.hpp"
#include "communication/create_transaction.hpp"
#include "global/globals.hpp"
#include <spdlog/spdlog.h>
namespace chainserver {

Chainstate::Chainstate(const ChainDB& db, BatchRegistry& br)
    : Chainstate(db.get_consensus_headers(), db, br)
{
}

Chainstate::Chainstate(
    std::tuple<std::vector<Batch>, HistoryHeights, State64Heights> init,
    const ChainDB& db,
    BatchRegistry& br)
    : db(db)
    , headerchain(std::move(std::get<0>(init)), br)
    , historyOffsets(std::move(std::get<1>(init)))
    , stateOffsets(std::move(std::get<2>(init)))
    , chainTxIds(db.fetch_tx_ids(length()))
{
    assert(this->historyOffsets.size() == headerchain.length());
    spdlog::info("Cache has {} entries", chainTxIds.size());
}

void Chainstate::assert_equal_length()
{
    assert(historyOffsets.size() == headerchain.length());
    assert(stateOffsets.size() == headerchain.length());
}

void Chainstate::fork(Chainstate::ForkData&& fd)
{

    const auto forkHeight { fd.rollbackResult.shrink.length.add1() };
    assert(fd.rollbackResult.shrink.length < length());
    // increment descriptor
    dsc += 1;

    // adapt header chain and offsets
    headerchain = std::move(fd.stage);
    historyOffsets.shrink(fd.rollbackResult.shrink.length);
    historyOffsets.append_vector(fd.appendResult.newHistoryOffsets);
    stateOffsets.shrink(fd.rollbackResult.shrink.length);
    stateOffsets.append_vector(fd.appendResult.stateOffsets);
    assert_equal_length();

    //////////////////////////////
    // erase mempool after rollback height
    _mempool.erase_from_height(forkHeight);

    //////////////////////////////
    // inform mempool about balance changes
    auto balanceUpdates { std::move(fd.appendResult.freeBalanceUpdates) };
    balanceUpdates.merge(std::move(fd.rollbackResult.freeBalanceUpdates));
    update_free_balances(balanceUpdates);

    //////////////////////////////
    // insert transactions into mempool
    DBCache dbCache(db);
    for (auto& tx : fd.rollbackResult.toMempool) {
        AccountId fromId { tx.from_id() };
        if (fromId >= db.next_id64())
            continue;

        PinHeight ph { tx.pin_height() };
        assert(ph <= forkHeight - 1);
        auto hash { headers().hash_at(ph) };
        auto txhash { tx.txhash(hash) };

        if (!fd.appendResult.newTxIds.contains(tx.txid())) {
            TxHeight txh { ph, account_height(fromId) };
            // TODO_Shifu: We need to make sure that the account id's are always correct (in case of rollbacks)
            _mempool.insert_tx(tx, txh, txhash, dbCache);
        }
    }

    // set transaction ids
    chainTxIds = std::move(fd.rollbackResult.chainTxIds);
    chainTxIds.merge(std::move(fd.appendResult.newTxIds));

    // remove from mempool (do FULL scan)
    for (auto& tid : chainTxIds)
        _mempool.erase(tid);

    // prune transaction ids
    prune_txids();
}

auto Chainstate::rollback(const RollbackResult& rb) -> HeaderchainRollback
{
    const auto forkHeight { rb.shrink.length.add1() };
    assert(rb.shrink.length < length());

    // increment descriptor
    dsc += 1;

    // adapt header chain and offsets
    headerchain.shrink(rb.shrink.length);
    historyOffsets.shrink(rb.shrink.length);
    stateOffsets.shrink(rb.shrink.length);
    assert_equal_length();

    // set transaction ids
    chainTxIds = std::move(rb.chainTxIds);

    // prune transaction ids
    prune_txids();

    // remove from mempool (do FULL scan)
    for (auto& tid : chainTxIds)
        _mempool.erase(tid);

    // erase mempool after rollback height
    _mempool.erase_from_height(forkHeight);

    //////////////////////////////
    // inform mempool about balance changes
    update_free_balances(rb.freeBalanceUpdates);

    //////////////////////////////
    // insert transactions into mempool
    AddressCache addressCache(db);
    DBCache dbCache(db);
    for (auto& tx : rb.toMempool) {
        AccountId fromId { tx.from_id() };

        PinHeight ph { tx.pin_height() };
        assert(ph <= forkHeight - 1);
        auto hash { headers().hash_at(ph) };
        TxHash txhash { tx.txhash(hash) };

        TxHeight txh { ph, account_height(fromId) };
        // TODO_Shifu: We need to make sure that the account id's are always correct (in case of rollbacks)
        _mempool.insert_tx(tx, txh, txhash, dbCache);
    }
    return HeaderchainRollback {
        .shrink { rb.shrink },
        .descriptor = dsc
    };
}

auto Chainstate::append(AppendMulti ad) -> HeaderchainAppend
{
    // safety check, length must increment
    const Height l { length() };
    assert(l <= ad.patchedChain.length());
    if (l != 0) {
        auto ll { l.nonzero_assert() };
        assert(ad.patchedChain[ll] == headerchain[ll]);
    }

    // adapt header chain and offsets
    headerchain = std::move(ad.patchedChain);
    historyOffsets.append_vector(ad.appendResult.newHistoryOffsets);
    stateOffsets.append_vector(ad.appendResult.stateOffsets);
    assert_equal_length();

    //////////////////////////////
    // inform mempool about balance changes
    update_free_balances(ad.appendResult.freeBalanceUpdates);

    // remove from mempool
    // remove outdated transactions
    auto nextBlockPinBegin { (ad.patchedChain.length() + 1).pin_begin() };
    _mempool.erase_pinned_before_height(nextBlockPinBegin);
    // remove used transactions
    for (const auto& tid : ad.appendResult.newTxIds)
        _mempool.erase(tid);

    // merge transaction ids
    chainTxIds.merge(std::move(ad.appendResult.newTxIds));

    // prune transaction ids
    prune_txids();

    // return append message
    return headers().get_append(l);
}

auto Chainstate::append(AppendSingle d) -> HeaderchainAppend
{
    const Height l { length() };

    // adapt header chain and offsets
    headerchain.append(d.prepared, *global().batchRegistry);
    historyOffsets.append(d.newHistoryOffset);
    stateOffsets.append(d.newStateOffset);
    assert_equal_length();

    //////////////////////////////
    // inform mempool about balance changes
    update_free_balances(d.freeBalanceUpdates);

    // remove from mempool
    // remove outdated transactions
    auto nextBlockPinBegin { (l + 1).pin_begin() };
    _mempool.erase_pinned_before_height(nextBlockPinBegin);
    // remove used transactions
    for (auto& tid : d.newTxIds)
        _mempool.erase(tid);

    // merge transaction ids
    chainTxIds.merge(std::move(d.newTxIds));

    // prune transaction ids
    prune_txids();

    // return append message
    return headers().get_append(l);
}

size_t Chainstate::on_mempool_constraint_update(){
    return _mempool.on_constraint_update();
};

auto Chainstate::insert_txs(const std::vector<TransactionMessage>& txs) -> std::vector<Error>
{
    DBCache c(db);
    std::vector<Error> res;
    res.reserve(txs.size());
    for (auto& tx : txs) {
        try {
            insert_tx(tx, c);
            res.push_back(0);
        } catch (const Error& e) {
            res.push_back(e.code);
        }
    }
    return res;
}

TxHash Chainstate::insert_tx(const TransactionMessage& tm, DBCache& wc)
{
    if (tm.pin_height() < (length() + 1).pin_begin())
        throw Error(EPINHEIGHT);
    if (txids().contains(tm.txid()))
        throw Error(ENONCE);
    auto h = headers().get_hash(tm.pin_height());
    if (!h)
        throw Error(EPINHEIGHT);
    auto txHash { tm.txhash(*h) };

    auto fromAddr = db.lookup_address(tm.from_id());
    if (!fromAddr)
        throw Error(EACCIDNOTFOUND);
    if (tm.compact_fee() < config().minMempoolFee)
        throw Error(EMINFEE);
    if (tm.from_address(txHash) != fromAddr)
        throw Error(EFAKEACCID);

    TxHeight th(tm.pin_height(), account_height(tm.from_id()));

    return insert_tx_internal(tm, th, txHash, wc, *fromAddr);
}

TxHash Chainstate::create_tx(const WartTransferCreate& m)
{
    PinHeight pinHeight = m.pin_height();
    if (pinHeight > length())
        throw Error(EPINHEIGHT);
    if (pinHeight < (length() + 1).pin_begin())
        throw Error(EPINHEIGHT);
    if (m.wart().is_zero())
        throw Error(EZEROAMOUNT);
    auto pinHash = headers().hash_at(pinHeight);
    auto txHash { m.tx_hash(pinHash) };
    auto fromAddr = m.from_address(txHash);
    if (fromAddr == m.to_addr())
        throw Error(ESELFSEND);
    auto accId = db.lookup_account(fromAddr);
    if (!accId)
        throw Error(EADDRNOTFOUND);

    TxHeight th(pinHeight, account_height(*accId));

    TransactionId txid(*accId, pinHeight, m.nonce_id());
    if (txids().contains(txid))
        throw Error(ENONCE);

    WartTransferMessage pm(txid, m.nonce_reserved(), m.compact_fee(), m.to_addr(), m.wart(), m.signature());

    DBCache c(db);
    return insert_tx_internal(std::move(pm), th, txHash, c, fromAddr);
}

void Chainstate::update_free_balances(const FreeBalanceUpdates& updates)
{
    // First set balance for nonWart because this will implicitly free some
    // frozen from mempool WART too, i.e. this order is superior in purging
    // as few entries as possible from the mempool.
    for (auto& [accountToken, balance] : updates.nonWart)
        _mempool.set_free_balance(accountToken, balance);
    for (auto& [accountToken, balance] : updates.wart)
        _mempool.set_free_balance(accountToken, balance);
}

TxHash Chainstate::insert_tx_internal(const TransactionMessage& m, TxHeight th, TxHash txHash, DBCache& c, const Address fromAddr)
{
    // additional checks
    m.visit_overload(
        [&](const WartTransferMessage& m) {
            if (fromAddr == m.to_addr())
                throw Error(ESELFSEND);
        },
        [&](const TokenTransferMessage& m) {
            if (fromAddr == m.to_addr())
                throw Error(ESELFSEND);
        },
        [&](const auto&) {});
    _mempool.insert_tx_throw(TransactionMessage { std::move(m) }, th, txHash, c);
    return txHash;
}

void Chainstate::prune_txids()
{
    chainTxIds.prune(length());
}
mempool::Updates Chainstate::pop_mempool_updates()
{
    return _mempool.pop_updates();
}
}
