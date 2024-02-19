#include "mempool.hpp"
#include "chainserver/transaction_ids.hpp"
#include <algorithm>
namespace mempool {
bool BalanceEntry::set_avail(Funds amount)
{
    if (used > amount)
        return false;
    avail = amount;
    return true;
}

void BalanceEntry::lock(Funds amount)
{
    assert(amount <= remaining());
    used += amount;
}

void BalanceEntry::unlock(Funds amount)
{
    assert(used >= amount);
    used -= amount;
}

std::vector<TransferTxExchangeMessage> Mempool::get_payments(size_t n, std::vector<Hash>* hashes) const
{
    if (n == 0)
        return {};
    std::vector<TransferTxExchangeMessage> res;
    res.reserve(n);
    size_t i = 0;
    for (auto iter = byFee.begin(); iter != byFee.end(); ++iter) {
        auto& [txid, entry] { **iter };
        res.push_back({ txid, entry });
        if (hashes)
            hashes->emplace_back((*iter)->second.hash);
        if (++i >= n)
            break;
    }
    return res;
}

void Mempool::apply_log(const Log& log)
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
    erase(a.entry.first);
    auto p = txs.emplace(a.entry);
    assert(p.second);
    byPin.insert(p.first);
    byFee.insert(p.first);
    byHash.insert(p.first);
}

void Mempool::apply_logevent(const Erase& e)
{
    erase(e.id);
}

std::optional<TransferTxExchangeMessage> Mempool::operator[](const TransactionId& id) const
{
    auto iter = txs.find(id);
    if (iter == txs.end())
        return {};
    return TransferTxExchangeMessage { iter->first, iter->second };
}

std::optional<TransferTxExchangeMessage> Mempool::operator[](const HashView txHash) const
{
    auto iter = byHash.find(txHash);
    if (iter == byHash.end())
        return {};
    assert((*iter)->second.hash == txHash);
    return TransferTxExchangeMessage { (*iter)->first, (*iter)->second };
}

bool Mempool::erase(Txmap::iterator iter, BalanceEntries::iterator b_iter, bool gc)
{
    const TransactionId id { iter->first };
    byPin.erase(iter);
    byFee.erase(iter);
    byHash.erase(iter);
    auto tx = *iter;
    Funds spend = tx.second.fee.uncompact() + tx.second.amount;
    txs.erase(iter);
    if (master)
        log.push_back(Erase { id });
    if (b_iter != balanceEntries.end()) {
        auto& balanceEntry { b_iter->second };
        balanceEntry.unlock(spend);
        if (gc && balanceEntry.is_clean()) {
            balanceEntries.erase(b_iter);
            return true;
        }
    }
    return false;
}

void Mempool::erase(decltype(txs)::iterator iter)
{
    auto b_iter = balanceEntries.find(iter->first.accountId);
    erase(iter, b_iter);
}

void Mempool::erase_from_height(Height h)
{
    auto iter = byPin.lower_bound(h);
    while (iter != byPin.end())
        erase(*(iter++));
}

void Mempool::erase_before_height(Height h)
{
    auto end = byPin.lower_bound(h);
    for (auto iter = byPin.begin(); iter != end;)
        erase(*(iter++));
}

void Mempool::erase(TransactionId id)
{
    if (auto iter = txs.find(id); iter != txs.end())
        erase(iter);
}

std::vector<TxidWithFee> Mempool::take(size_t N) const
{
    std::vector<TxidWithFee> out;
    for (auto p : txs) {
        if (out.size() == N)
            break;
        out.push_back({ p.first, p.second.fee });
    }
    return out;
}

std::vector<TransactionId> Mempool::filter_new(const std::vector<TxidWithFee>& v) const
{
    std::vector<TransactionId> out;
    for (auto& t : v) {
        auto iter = txs.find(t.txid);
        if (iter == txs.end()) {
            if (t.fee >= min_fee())
                out.push_back(t.txid);
        } else if (t.fee > iter->second.fee)
            out.push_back(t.txid);
    }
    return out;
}

void Mempool::set_balance(AccountId accId, Funds newBalance)
{
    auto b_iter { balanceEntries.find(accId) };
    if (b_iter == balanceEntries.end())
        return;
    auto& balanceEntry { b_iter->second };
    if (balanceEntry.set_avail(newBalance))
        return;

    auto iterators { txs.by_fee_inc(accId) };

    for (size_t i = 0; i < iterators.size(); ++i) {
        bool allErased = erase(iterators[i], b_iter);
        assert(allErased == (i == iterators.size() - 1));
        if (balanceEntry.set_avail(newBalance))
            return;
    }
    assert(false); // should not happen
}

int32_t Mempool::insert_tx(const TransferTxExchangeMessage& pm,
    TransactionHeight txh,
    const TxHash& txhash,
    const AddressFunds& af)
{
    if (pm.from_address(txhash) != af.address)
        return EFAKEACCID;

    if (af.funds.is_zero())
        return EBALANCE;
    auto balanceIter = balanceEntries.try_emplace(pm.from_id(), af).first;
    auto& e { balanceIter->second };

    const Funds spend = pm.spend();
    if (spend.overflow())
        return EBALANCE;

    { // check if we can delete enough old entries to insert new entry
        std::vector<Txmap::iterator> clear;
        std::optional<Txmap::iterator> match;
        Funds clearSum { 0 };
        if (auto iter = txs.find(pm.txid); iter != txs.end()) {
            if (iter->second.fee >= pm.compactFee) {
                return ENONCE;
            }
            clear.push_back(iter);
            match = iter;
        }
        const auto remaining { e.remaining() };
        if (remaining + clearSum < spend) {
            auto iterators { txs.by_fee_inc(pm.txid.accountId) };
            for (auto iter : iterators) {
                if (iter == match)
                    continue;
                if (iter->second.fee >= pm.fee())
                    break;
                clear.push_back(iter);
                clearSum += iter->second.spend();
                if (remaining + clearSum >= spend) {
                    goto candelete;
                }
            }
            return EBALANCE;
        candelete:;
        }
        for (auto& iter : clear)
            erase(iter, balanceIter, false); // make sure we don't delete balanceIter
    }

    e.lock(spend);
    auto [iter, inserted] = txs.try_emplace(pm.txid,
        pm.reserved, pm.compactFee, pm.toAddr, pm.amount, pm.signature, txhash, txh);
    assert(inserted);
    if (master)
        log.push_back(Put { *iter });
    byPin.insert(iter);
    byFee.insert(iter);
    byHash.insert(iter);
    prune();
    return 0;
}

void Mempool::prune()
{
    while (size() > maxSize) {
        erase(*byFee.rend()); // delete smallest element
    }
}

CompactUInt Mempool::min_fee() const
{
    if (size() < maxSize)
        return 0;
    return (*byFee.rend())->second.fee.next();
}

}
