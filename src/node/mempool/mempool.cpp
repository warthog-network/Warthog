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
    assert(amount <= free());
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
    assert(index.insert(p.first));
    // assert(byPin.insert(p.first).second);
    // assert(byHash.insert(p.first).second);
    assert(byFee.insert(p.first));
    assert(byToken.insert(p.first));
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
    auto iter = index.hash().find(txHash);
    if (iter == index.hash().end())
        return {};
    assert((*iter)->txhash == txHash);
    return *static_cast<const TransactionMessage*>(&**iter);
}

bool Mempool::erase_internal(Txset::const_iter_t iter, BalanceEntries::iterator b_iter, bool gc)
{
    assert(size() == index.size());
    // assert(size() == byPin.size());
    // assert(size() == byHash.size());
    assert(size() == byFee.size());
    assert(size() == byToken.size());

    // copy before erase
    const TransactionId id { iter->txid() };
    Wart spend { iter->spend_wart_assert() };

    // erase iter and its references

    assert(index.erase(iter) == 1);
    // assert(byHash.erase(iter) == 1);
    // assert(byPin.erase(iter) == 1);
    assert(byFee.erase(iter) == 1);
    assert(byToken.erase(iter) == 1);
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

void Mempool::erase_internal(Txset::const_iter_t iter)
{
    auto b_iter = lockedBalances.find({ iter->from_id(), iter->altToken });
    erase_internal(iter, b_iter);
}

void Mempool::erase_from_height(Height h)
{
    auto iter { index.pin().lower_bound(h) };
    while (iter != index.pin().end())
        erase_internal(*(iter++));
}

void Mempool::erase_before_height(Height h)
{
    auto end = index.pin().lower_bound(h);
    for (auto iter = index.pin().begin(); iter != end;)
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

void Mempool::set_free_balance(AccountToken at, Wart newBalance)
{
    auto b_iter { lockedBalances.find(at) };
    if (b_iter == lockedBalances.end())
        return;
    auto& balanceEntry { b_iter->second };
    if (balanceEntry.set_avail(newBalance))
        return;

    auto iterators { txs.by_fee_inc(at.account_id()) };

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

Error Mempool::insert_tx(const TransactionMessage& pm, TxHeight txh, const TxHash& hash, chainserver::DBCache& cache)
{
    try {
        insert_tx_throw(pm, txh, hash, cache);
        return 0;
    } catch (Error e) {
        return e;
    }
}

std::optional<TokenFunds> Mempool::token_spend_throw(const TransactionMessage& pm, chainserver::DBCache& cache) const
{
    TokenId tokenId { TokenId::WART };
    if (auto s { pm.spend_token_throw() }) {
        auto pAsset { cache.assetsByHash.lookup(s->hash) };
        if (pAsset == nullptr)
            throw Error(EASSETHASHNOTFOUND);
        auto& asset { *pAsset };
        tokenId = asset.id.token_id(s->isLiquidity);
        return TokenFunds { tokenId, s->amount };
    }
    return {};
}
auto Mempool::get_balance(AccountToken at, chainserver::DBCache& cache) -> LockedBalance
{
    auto balanceIter { lockedBalances.upper_bound(at) };
    if (balanceIter == lockedBalances.end() || balanceIter->first != at) {
        // need to insert
        auto total { cache.balance[at] };
        return LockedBalance(total);
        // balanceIter = lockedBalances.emplace_hint(balanceIter, at, bal);
    }
    return balanceIter->second;
}

void Mempool::insert_tx_throw(const TransactionMessage& pm,
    TxHeight txh,
    const TxHash& txhash, chainserver::DBCache& cache)
{

    auto fromId { pm.from_id() };

    std::optional<Txset::const_iter_t> match;
    std::vector<Txset::const_iter_t> clear;
    const auto& t { txs };
    if (auto iter = t().find(pm.txid()); iter != t().end()) {
        if (iter->compact_fee() >= pm.compact_fee()) {
            throw Error(ENONCE);
        }
        clear.push_back(iter);
        match = iter;
    }

    const Wart wartSpend { pm.spend_wart_throw() };
    auto wartBal { get_balance({ pm.from_id(), TokenId::WART }, cache) };
    if (wartBal.total() < wartSpend)
        throw Error(EBALANCE);

    size_t token_idx0 { clear.size() };
    if (auto ts { token_spend_throw(pm, cache) }) { // if this transaction spends tokens different from WART
        // first make sure we can delete enough elements from the
        // mempool to cover the amount of nonwart tokens needed
        // for this transaction
        AccountToken at { fromId, ts->id };
        auto tokenSpend { ts->amount };
        auto tokenBal { get_balance(at, cache) };
        if (tokenBal.total() < tokenSpend)
            throw Error(ETOKBALANCE);
        auto& set { index.account_token_fee() };
        // loop through the range where the AccountToken is equal
        for (auto it { set.lower_bound(at) };
            it != set.end() && (*it)->altToken == at.token_id() && (*it)->from_id() == fromId; ++it) {
            if (tokenBal.free() >= tokenSpend)
                break;
            auto iter = *it;
            if (iter == match)
                continue;
            if (iter->compact_fee() >= pm.compact_fee())
                break;
            clear.push_back(iter);
            wartBal.unlock(iter->spend_wart_assert());
            tokenBal.unlock(iter->spend_token_throw()->amount);
        }
        if (tokenBal.free() < tokenSpend)
            throw Error(ETOKBALANCE);
    }
    size_t token_idx1 { clear.size() };
    size_t i { token_idx0 };

    {
        AccountToken at { pm.from_id(), TokenId::WART };
        { // check if we can delete enough old entries to insert new entry
            if (wartBal.free() < wartSpend) {
                auto iterators { txs.by_fee_inc(pm.txid().accountId) };
                for (auto iter : iterators) {
                    if (iter == match)
                        continue;
                    if (i < token_idx1) {
                        if (clear[i] == iter) {
                            // iter already inserted in token balance loop
                            i += 1;
                            continue;
                        };
                    }
                    if (iter->compact_fee() >= pm.compact_fee())
                        break;
                    clear.push_back(iter);
                    wartBal.unlock(iter->spend_wart_assert());
                    if (wartBal.free() >= wartSpend)
                        goto candelete;
                }
                throw Error(EBALANCE);
            candelete:;
            }
            for (auto& iter : clear)
                erase_internal(iter, balanceIter, false); // make sure we don't delete balanceIter
        }
        wartBal.lock(wartSpend);
    }

    auto [iter, inserted] = txs().insert(Entry { pm, txhash, txh, token_spend_throw });
    assert(inserted);
    if (master)
        updates.push_back(Put { *iter });
    index.insert(iter);
    assert(index.insert(iter));
    // assert(byPin.insert(iter).second);
    // assert(byHash.insert(iter).second);
    assert(byFee.insert(iter));
    assert(byToken.insert(iter));
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
