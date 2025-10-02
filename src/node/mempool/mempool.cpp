#include "mempool.hpp"
#include "api/events/emit.hpp"
#include "chainserver/state/helpers/cache.hpp"
// #include "api/events/emit.hpp"
namespace mempool {
bool LockedBalance::set_avail(Wart amount)
{
    if (used > amount)
        return false;
    avail = amount;
    return true;
}

void LockedBalance::lock(Wart amount)
{
    assert(amount <= remaining());
    used.add_assert(amount);
}

void LockedBalance::unlock(Wart amount)
{
    assert(used >= amount);
    used.subtract_assert(amount);
}

std::vector<TransactionMessage> Mempool::get_transactions(size_t n, NonzeroHeight height, std::vector<TxHash>* hashes) const
{
    std::vector<TransactionMessage> res;
    res.reserve(n);
    constexpr uint32_t fivedaysBlocks = 5 * 24 * 60 * 3;
    constexpr uint32_t unblockXeggexHeight = 2576442 + fivedaysBlocks;

    std::set<TransactionId> tx_txids;
    std::set<TransactionId> cancel_txids;
    for (auto txiter : byFee) {
        if (res.size() >= n)
            break;
        if (height.value() <= unblockXeggexHeight && txiter->from_id().value() == 1910)
            continue;
        auto& tx { *txiter };
        auto id { tx.txid() };
        if (tx_txids.contains(id) || cancel_txids.contains(id))
            continue;
        if (tx.holds<CancelationMessage>()) {
            auto cid { tx.get<CancelationMessage>().cancel_txid() };
            assert(cid != id); // should be ensured in CancelationMessage::throw_if_bad
            if (tx_txids.contains(cid))
                continue;
            cancel_txids.insert(cid);
        }
        tx_txids.insert(id);

        res.push_back(tx);
        if (hashes)
            hashes->emplace_back(tx.txhash);
    }
    return res;
}

void Mempool::apply_log(const Updates& log)
{
    for (auto& l : log) {
        std::visit([&](auto& entry) {
            apply_logevent(entry);
        },
            l);
    }
}

void Mempool::apply_logevent(const Put& a)
{
    erase(a.entry.txid());
    auto p = txs().insert(std::move(a.entry));
    api::event::emit_mempool_add(a, txs.size());
    assert(p.second);
    assert(byPin.insert(p.first).second);
    assert(byFee.insert(p.first));
    assert(byHash.insert(p.first).second);
}

void Mempool::apply_logevent(const Erase& e)
{
    erase(e.id);
    api::event::emit_mempool_erase(e, size());
}

std::optional<TransactionMessage> Mempool::operator[](const TransactionId& id) const
{
    auto iter = txs().find(id);
    if (iter == txs().end())
        return {};
    return *static_cast<const TransactionMessage*>(&*iter);
}

std::optional<TransactionMessage> Mempool::operator[](const HashView txHash) const
{
    auto iter = byHash.find(txHash);
    if (iter == byHash.end())
        return {};
    assert((*iter)->txhash == txHash);
    return *static_cast<const TransactionMessage*>(&**iter);
}

bool Mempool::erase_internal(Txset::const_iterator iter, BalanceEntries::iterator b_iter, bool gc)
{
    assert(size() == byFee.size());
    assert(size() == byPin.size());
    assert(size() == byHash.size());

    // copy before erase
    const TransactionId id { iter->txid() };
    Wart spend { iter->spend_wart_assert() };

    // erase iter and its references
    assert(byPin.erase(iter) == 1);
    assert(byFee.erase(iter) == 1);
    assert(byHash.erase(iter) == 1);
    txs().erase(iter);

    if (master)
        updates.push_back(Erase { id });

    // update locked balance
    if (b_iter != lockedBalances.end()) {
        auto& balanceEntry { b_iter->second };
        balanceEntry.unlock(spend);
        if (gc && balanceEntry.is_clean()) {
            lockedBalances.erase(b_iter);
            return true;
        }
    }
    return false;
}

void Mempool::erase_internal(Txset::const_iterator iter)
{
    auto b_iter = lockedBalances.find(iter->from_id());
    erase_internal(iter, b_iter);
}

void Mempool::erase_from_height(Height h)
{
    auto iter = byPin.lower_bound(h);
    while (iter != byPin.end())
        erase_internal(*(iter++));
}

void Mempool::erase_before_height(Height h)
{
    auto end = byPin.lower_bound(h);
    for (auto iter = byPin.begin(); iter != end;)
        erase_internal(*(iter++));
}

void Mempool::erase(TransactionId id)
{
    auto& t { txs() };
    if (auto iter = t.find(id); iter != t.end())
        erase_internal(iter);
}

std::vector<TxidWithFee> Mempool::sample(size_t N) const
{
    auto sampled { byFee.sample(800, N) };
    std::vector<TxidWithFee> out;
    for (auto iter : sampled) {
        out.push_back({ iter->txid(), iter->compact_fee() });
    }
    return out;
}

std::vector<TransactionId> Mempool::filter_new(const std::vector<TxidWithFee>& v) const
{
    std::vector<TransactionId> out;
    for (auto& t : v) {
        auto iter = txs().find(t.txid);
        if (iter == txs().end()) {
            if (t.fee >= min_fee())
                out.push_back(t.txid);
        } else if (t.fee > iter->compact_fee())
            out.push_back(t.txid);
    }
    return out;
}

void Mempool::set_wart_balance(AccountId aid, Wart newBalance)
{
    auto b_iter { lockedBalances.find(aid) };
    if (b_iter == lockedBalances.end())
        return;
    auto& balanceEntry { b_iter->second };
    if (balanceEntry.set_avail(newBalance))
        return;

    auto iterators { txs.by_fee_inc(aid) };

    for (size_t i = 0; i < iterators.size(); ++i) {
        bool allErased = erase_internal(iterators[i], b_iter);
        bool lastIteration = (i == iterators.size() - 1);
        assert(allErased == lastIteration);
        // balanceEntry reference is invalidateed when all entries are erased
        // because it will be wiped together with last entry.
        if (allErased || balanceEntry.set_avail(newBalance))
            return;
    }
    assert(false); // should not happen
}

Error Mempool::insert_tx(const TransactionMessage& pm, TxHeight txh, const TxHash& hash, chainserver::WartCache& wartCache)
{
    try {
        insert_tx_throw(pm, txh, hash, wartCache);
        return 0;
    } catch (Error e) {
        return e;
    }
}

void Mempool::insert_tx_throw(const TransactionMessage& pm,
    TxHeight txh,
    const TxHash& txhash, chainserver::WartCache& wartCache)
{

    auto fromId { pm.from_id() };
    auto balanceIter { lockedBalances.upper_bound(pm.from_id()) };
    if (balanceIter == lockedBalances.end() || balanceIter->first != pm.from_id()) {
        // need to insert
        auto wart { wartCache[fromId] };
        if (wart.is_zero())
            throw Error(EBALANCE);
        balanceIter = lockedBalances.emplace_hint(balanceIter, pm.from_id(), wart);
    }
    auto& e { balanceIter->second };
    const Wart spend { pm.spend_wart_throw() };

    { // check if we can delete enough old entries to insert new entry
        std::vector<Txset::const_iterator> clear;
        std::optional<Txset::const_iterator> match;
        const auto& t { txs };
        if (auto iter = t().find(pm.txid()); iter != t().end()) {
            if (iter->compact_fee() >= pm.compact_fee()) {
                throw Error(ENONCE);
            }
            clear.push_back(iter);
            match = iter;
        }
        const auto remaining { e.remaining() };
        if (remaining < spend) {
            Wart clearSum { Wart::zero() };
            auto iterators { txs.by_fee_inc(pm.txid().accountId) };
            for (auto iter : iterators) {
                if (iter == match)
                    continue;
                if (iter->compact_fee() >= pm.compact_fee())
                    break;
                clear.push_back(iter);
                clearSum.add_assert(iter->spend_wart_assert());
                if (Wart::sum_assert(remaining, clearSum) >= spend) {
                    goto candelete;
                }
            }
            throw Error(EBALANCE);
        candelete:;
        }
        for (auto& iter : clear)
            erase_internal(iter, balanceIter, false); // make sure we don't delete balanceIter
    }

    e.lock(spend);
    auto [iter, inserted] = txs().insert(Entry { pm, txhash, txh });
    assert(inserted);
    if (master)
        updates.push_back(Put { *iter });
    assert(byPin.insert(iter).second);
    assert(byFee.insert(iter));
    assert(byHash.insert(iter).second);
    prune();
}

void Mempool::prune()
{
    while (size() > maxSize)
        erase_internal(byFee.smallest()); // delete smallest element
}

CompactUInt Mempool::min_fee() const
{
    if (size() < maxSize)
        return CompactUInt::smallest();
    return byFee.smallest()->compact_fee().next();
}

}
