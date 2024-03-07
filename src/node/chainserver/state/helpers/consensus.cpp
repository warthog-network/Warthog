#include "consensus.hpp"
#include "communication/create_payment.hpp"
#include "db/chain_db.hpp"
#include "global/globals.hpp"
#include <spdlog/spdlog.h>
namespace chainserver {

Chainstate::Chainstate(const ChainDB& db, BatchRegistry& br)
    : Chainstate(db.getConsensusHeaders(), db, br)
{
}

Chainstate::Chainstate(
    std::tuple<std::vector<Batch>, HistoryHeights, AccountHeights> init,
    const ChainDB& db,
    BatchRegistry& br)
    : db(db)
    , headerchain(std::move(std::get<0>(init)), br)
    , historyOffsets(std::move(std::get<1>(init)))
    , accountOffsets(std::move(std::get<2>(init)))
    , chainTxIds(db.fetch_tx_ids(length()))
{
    assert(this->historyOffsets.size() == headerchain.length());
    spdlog::info("Cache has {} entries", chainTxIds.size());
}

void Chainstate::assert_equal_length()
{
    assert(historyOffsets.size() == headerchain.length());
    assert(accountOffsets.size() == headerchain.length());
}

void Chainstate::fork(Chainstate::ForkData&& fd)
{

    const auto forkHeight { fd.rollbackResult.shrinkLength + 1 };
    assert(fd.rollbackResult.shrinkLength < length());
    // increment descriptor
    dsc += 1;

    // adapt header chain and offsets
    headerchain = std::move(fd.stage);
    historyOffsets.shrink(fd.rollbackResult.shrinkLength);
    historyOffsets.append_vector(fd.appendResult.newHistoryOffsets);
    accountOffsets.shrink(fd.rollbackResult.shrinkLength);
    accountOffsets.append_vector(fd.appendResult.newAccountOffsets);
    assert_equal_length();

    //////////////////////////////
    // erase mempool after rollback height
    _mempool.erase_from_height(forkHeight);

    //////////////////////////////
    // inform mempool about balance changes
    auto balanceUpdates { std::move(fd.appendResult.balanceUpdates) };
    balanceUpdates.merge(fd.rollbackResult.balanceUpdates);
    for (auto& [accountId, balance] : balanceUpdates)
        _mempool.set_balance(accountId, balance);

    //////////////////////////////
    // insert transactions into mempool
    AccountCache accountCache(db);
    for (auto& tx : fd.rollbackResult.toMempool) {
        AccountId fromId { tx.from_id() };
        auto& from { accountCache[fromId] };

        PinHeight ph { tx.pin_height() };
        assert(ph <= forkHeight - 1);
        Hash hash { headers().hash_at(ph) };
        TxHash txhash { tx.txhash(hash) };

        if (!fd.appendResult.newTxIds.contains(tx.txid)) {
            TransactionHeight txh { ph, account_height(fromId) };
            _mempool.insert_tx(tx, txh, txhash, from);
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
    const auto forkHeight { rb.shrinkLength + 1 };
    assert(rb.shrinkLength < length());

    // increment descriptor
    dsc += 1;

    // adapt header chain and offsets
    headerchain.shrink(rb.shrinkLength);
    historyOffsets.shrink(rb.shrinkLength);
    accountOffsets.shrink(rb.shrinkLength);
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
    auto& balanceUpdates { rb.balanceUpdates };
    for (auto& [accountId, balance] : balanceUpdates)
        _mempool.set_balance(accountId, balance);

    //////////////////////////////
    // insert transactions into mempool
    AccountCache accountCache(db);
    for (auto& tx : rb.toMempool) {
        AccountId fromId { tx.from_id() };
        auto& from { accountCache[fromId] };

        PinHeight ph { tx.pin_height() };
        assert(ph <= forkHeight - 1);
        Hash hash { headers().hash_at(ph) };
        TxHash txhash { tx.txhash(hash) };

        TransactionHeight txh { ph, account_height(fromId) };
        _mempool.insert_tx(tx, txh, txhash, from);
    }
    return HeaderchainRollback {
        .shrinkLength { rb.shrinkLength },
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
    accountOffsets.append_vector(ad.appendResult.newAccountOffsets);
    assert_equal_length();

    //////////////////////////////
    // inform mempool about balance changes
    auto& balanceUpdates { ad.appendResult.balanceUpdates };
    for (auto& [accountId, balance] : balanceUpdates)
        _mempool.set_balance(accountId, balance);

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
    accountOffsets.append(d.newAccountOffset);
    assert_equal_length();

    //////////////////////////////
    // inform mempool about balance changes
    auto& balanceUpdates { d.balanceUpdates };
    for (auto& [accountId, balance] : balanceUpdates)
        _mempool.set_balance(accountId, balance);

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

TxHash Chainstate::insert_tx(const TransferTxExchangeMessage& pm)
{
    if (pm.pin_height() < (length() + 1).pin_begin())
        throw Error(EPINHEIGHT);
    if (txids().contains(pm.txid))
        throw Error(ENONCE);
    auto h = headers().get_hash(pm.pin_height());
    if (!h)
        throw Error(EPINHEIGHT);
    if (pm.amount.is_zero())
        throw Error(EZEROAMOUNT);
    auto txHash { pm.txhash(*h) };
    if (pm.from_address(txHash) == pm.toAddr)
        throw Error(ESELFSEND);

    auto p = db.lookup_account(pm.from_id());
    if (!p)
        throw Error(ENOTFOUND);
    TransactionHeight th(pm.pin_height(), account_height(pm.from_id()));
    if (auto e { _mempool.insert_tx(pm, th, txHash, *p) }; e != 0)
        throw Error(e);
    return txHash;
}

TxHash Chainstate::insert_tx(const PaymentCreateMessage& m)
{
    PinHeight pinHeight = m.pinHeight;
    if (pinHeight > length())
        throw Error(EPINHEIGHT);
    if (pinHeight < (length() + 1).pin_begin())
        throw Error(EPINHEIGHT);
    if (m.amount.is_zero())
        throw Error(EZEROAMOUNT);
    auto pinHash = headers().hash_at(pinHeight);
    auto txHash { m.tx_hash(pinHash) };
    auto fromAddr = m.from_address(txHash);
    if (fromAddr == m.toAddr)
        throw Error(ESELFSEND);
    auto p = db.lookup_address(fromAddr);
    if (!p)
        throw Error(ENOTFOUND);
    auto& [accountId, balance] = *p;
    AddressFunds af { fromAddr, balance };
    TransferTxExchangeMessage pm(accountId, m);
    if (txids().contains(pm.txid))
        throw Error(ENONCE);
    TransactionHeight th(pinHeight, account_height(accountId));
    if (auto e { _mempool.insert_tx(pm, th, txHash, af) }; e != 0)
        throw Error(e);
    return txHash;
}

void Chainstate::prune_txids()
{
    chainTxIds.prune(length());
}
mempool::Log Chainstate::pop_mempool_log()
{
    return _mempool.pop_log();
}
}
