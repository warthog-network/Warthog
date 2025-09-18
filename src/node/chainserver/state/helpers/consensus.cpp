#include "consensus.hpp"
#include "cache.hpp"
#include "chainserver/db/chain_db.hpp"
#include "communication/create_transaction.hpp"
#include "global/globals.hpp"
#include <spdlog/spdlog.h>
namespace chainserver {

Chainstate::Chainstate(const ChainDB& db, BatchRegistry& br)
    : Chainstate(db.getConsensusHeaders(), db, br)
{
}

Chainstate::Chainstate(
    std::tuple<std::vector<Batch>, HistoryHeights, State32Heights> init,
    const ChainDB& db,
    BatchRegistry& br)
    : db(db)
    , headerchain(std::move(std::get<0>(init)), br)
    , historyOffsets(std::move(std::get<1>(init)))
    , state32Offsets(std::move(std::get<2>(init)))
    , chainTxIds(db.fetch_tx_ids(length()))
{
    assert(this->historyOffsets.size() == headerchain.length());
    spdlog::info("Cache has {} entries", chainTxIds.size());
}

void Chainstate::assert_equal_length()
{
    assert(historyOffsets.size() == headerchain.length());
    assert(state32Offsets.size() == headerchain.length());
}

void Chainstate::fork(Chainstate::ForkData&& fd)
{

    const auto forkHeight { fd.rollbackResult.shrink.length + 1 };
    assert(fd.rollbackResult.shrink.length < length());
    // increment descriptor
    dsc += 1;

    // adapt header chain and offsets
    headerchain = std::move(fd.stage);
    historyOffsets.shrink(fd.rollbackResult.shrink.length);
    historyOffsets.append_vector(fd.appendResult.newHistoryOffsets);
    state32Offsets.shrink(fd.rollbackResult.shrink.length);
    state32Offsets.append_vector(fd.appendResult.state32Offsets);
    assert_equal_length();

    //////////////////////////////
    // erase mempool after rollback height
    _mempool.erase_from_height(forkHeight);

    //////////////////////////////
    // inform mempool about balance changes
    auto balanceUpdates { std::move(fd.appendResult.wartUpdates) };
    balanceUpdates.merge(fd.rollbackResult.wartUpdates);
    for (auto& [accountId, balance] : balanceUpdates)
        _mempool.set_wart_balance(accountId, balance);

    //////////////////////////////
    // insert transactions into mempool
    WartCache wartCache(db);
    for (auto& tx : fd.rollbackResult.toMempool) {
        AccountId fromId { tx.from_id() };
        if (fromId >= db.next_id32())
            continue;

        PinHeight ph { tx.pin_height() };
        assert(ph <= forkHeight - 1);
        auto hash { headers().hash_at(ph) };
        auto txhash { tx.txhash(hash) };

        if (!fd.appendResult.newTxIds.contains(tx.txid())) {
            TxHeight txh { ph, account_height(fromId) };
            // TODO_Shifu: We need to make sure that the account id's are always correct (in case of rollbacks)
            _mempool.insert_tx(tx, txh, txhash, wartCache);
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
    const auto forkHeight { rb.shrink.length + 1 };
    assert(rb.shrink.length < length());

    // increment descriptor
    dsc += 1;

    // adapt header chain and offsets
    headerchain.shrink(rb.shrink.length);
    historyOffsets.shrink(rb.shrink.length);
    state32Offsets.shrink(rb.shrink.length);
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
    auto& balanceUpdates { rb.wartUpdates };
    for (auto& [accountId, wart] : balanceUpdates)
        _mempool.set_wart_balance(accountId, wart);

    //////////////////////////////
    // insert transactions into mempool
    AddressCache addressCache(db);
    WartCache wartCache(db);
    for (auto& tx : rb.toMempool) {
        AccountId fromId { tx.from_id() };

        PinHeight ph { tx.pin_height() };
        assert(ph <= forkHeight - 1);
        auto hash { headers().hash_at(ph) };
        TxHash txhash { tx.txhash(hash) };

        TxHeight txh { ph, account_height(fromId) };
        // TODO_Shifu: We need to make sure that the account id's are always correct (in case of rollbacks)
        _mempool.insert_tx(tx, txh, txhash, wartCache);
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
    state32Offsets.append_vector(ad.appendResult.state32Offsets);
    assert_equal_length();

    //////////////////////////////
    // inform mempool about balance changes
    auto& wartUpdates { ad.appendResult.wartUpdates };
    for (auto& [accountId, balance] : wartUpdates)
        _mempool.set_wart_balance(accountId, balance);

    // remove from mempool
    // remove outdated transactions
    auto nextBlockPinBegin { (ad.patchedChain.length() + 1).pin_begin() };
    _mempool.erase_before_height(nextBlockPinBegin);
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
    state32Offsets.append(d.newAccountOffset);
    assert_equal_length();

    //////////////////////////////
    // inform mempool about balance changes
    auto& wartUpdates { d.wartUpdates };
    for (auto& [accountId, balance] : wartUpdates)
        _mempool.set_wart_balance(accountId, balance);

    // remove from mempool
    // remove outdated transactions
    auto nextBlockPinBegin { (l + 1).pin_begin() };
    _mempool.erase_before_height(nextBlockPinBegin);
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

TxHash Chainstate::insert_tx(const TransactionMessage& tm, WartCache& wc)
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

    WartCache wc(db);
    return insert_tx_internal(std::move(pm), th, txHash, wc, fromAddr);
}

TxHash Chainstate::insert_tx_internal(const TransactionMessage& m, TxHeight th, TxHash txHash, WartCache wc, const Address fromAddr)
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
    _mempool.insert_tx_throw(TransactionMessage { std::move(m) }, th, txHash, wc);
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
