#include "mempool.hpp"
#include "chainserver/transaction_ids.hpp"
namespace mempool {

std::vector<TransferTxExchangeMessage> Mempool::get_payments(size_t n, std::vector<Hash>* hashes) const
{
    if (n == 0) {
        return {};
    }
    std::vector<TransferTxExchangeMessage> res;
    res.reserve(n);
    size_t i = 0;
    for (auto iter = byFee.rbegin(); iter != byFee.rend(); ++iter) {
        try {
            TransferTxExchangeMessage m { (*iter)->first, (*iter)->second };

            res.push_back(m);
            if (hashes)
                hashes->emplace_back((*iter)->second.hash);
        } catch (...) {
        }
        if (++i >= n)
            break;
    }
    return res;
};

void Mempool::apply_log(const Log& log)
{
    for (auto& l : log) {
        std::visit([&](auto& entry) {
            apply_logevent(entry);
        },
            l);
    }
};

void Mempool::apply_logevent(const Put& a)
{
    erase(a.entry.first);
    auto p = txs.emplace(a.entry);
    assert(p.second);
    byPin.insert(p.first);
    byFee.insert(p.first);
    byHash.insert(p.first);
};

void Mempool::apply_logevent(const Erase& e)
{
    erase(e.id);
};

std::optional<TransferTxExchangeMessage> Mempool::operator[](const TransactionId& id) const
{
    auto iter = txs.find(id);
    if (iter == txs.end())
        return {};
    return TransferTxExchangeMessage { iter->first, iter->second };
};

std::optional<TransferTxExchangeMessage> Mempool::operator[](const HashView txHash) const
{
    auto iter = byHash.find(txHash);
    if (iter == byHash.end())
        return {};
    assert((*iter)->second.hash == txHash);
    return TransferTxExchangeMessage { (*iter)->first, (*iter)->second };
};

void Mempool::erase(decltype(txs)::iterator iter)
{
    const TransactionId id { iter->first };
    byPin.erase(iter);
    byFee.erase(iter);
    byHash.erase(iter);
    auto tx = *iter;
    Funds spend = tx.second.fee.uncompact() + tx.second.amount;
    auto biter = balanceEntries.find(id.accountId);
    if (biter != balanceEntries.end()) {
        biter->second._used -= spend;
        if (biter->second._used.is_zero()) {
            balanceEntries.erase(biter);
        }
    }
    txs.erase(iter);
    if (master)
        log.push_back(Erase { id });
};

void Mempool::erase_from_height(Height h)
{
    auto iter = byPin.lower_bound(h);
    while (iter != byPin.end()) {
        erase(*(iter++));
    }
};

void Mempool::erase_before_height(Height h)
{
    auto end = byPin.lower_bound(h);
    for (auto iter = byPin.begin(); iter != end;) {
        erase(*(iter++));
    }
};

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
};

std::vector<TransactionId> Mempool::filter_new(const std::vector<TxidWithFee>& v) const
{
    std::vector<TransactionId> out;
    for (auto& t : v) {
        auto iter = txs.find(t.txid);
        if (iter == txs.end() || t.fee > iter->second.fee)
            out.push_back(t.txid);
    }
    return out;
};

int32_t Mempool::insert_tx(const TransferTxExchangeMessage& pm,
    TransactionHeight txh,
    const TxHash& txhash,
    const AddressFunds& af)
{
    if (pm.from_address(txhash) != af.address)
        return EFAKEACCID;

    if (af.funds.is_zero())
        return EBALANCE;
    auto& e = balanceEntries.try_emplace(pm.from_id(), af).first->second;

    assert(!e.avail.is_zero());
    Funds spend = pm.fee() + pm.amount;
    if (spend.overflow() || spend + e._used > e.avail)
        return EBALANCE;

    if (auto iter = txs.find(pm.txid); iter != txs.end()) {
        if (iter->second.fee >= pm.compactFee) {
            return ENONCE;
        }
        erase(iter);
    }

    auto [iter, inserted] = txs.try_emplace(pm.txid,
        pm.reserved, pm.compactFee, pm.toAddr, pm.amount, pm.signature, txhash, txh);
    assert(inserted);
    if (master)
        log.push_back(Put { *iter });
    byPin.insert(iter);
    byFee.insert(iter);
    byHash.insert(iter);
    e._used += spend;
    return 0;
};

}
