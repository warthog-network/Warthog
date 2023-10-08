#include "block_applier.hpp"
#include "block/body/parse.hpp"
#include "block/body/rollback.hpp"
#include "block/chain/header_chain.hpp"
#include "block/chain/history/history.hpp"
#include "db/chain_db.hpp"

namespace {
class BalanceChecker {

    class AccountFlow {
        friend class BalanceChecker;

    public:
        Funds in() const { return _in; }
        Funds out() const { return _out; }

    private:
        Funds _in { 0 };
        Funds _out { 0 };
        std::vector<size_t> referredPayout;
        std::vector<size_t> referredFrom;
        std::vector<size_t> referredTo;
    };

    class OldAccountFlow : public AccountFlow {
        friend class BalanceChecker;
        Address address;
    };

public:
    BalanceChecker(AccountId beginNewAccountId,
        const BodyView& bv, NonzeroHeight height)
        : beginNewAccountId(beginNewAccountId)
        , endNewAccountId(beginNewAccountId + bv.getNAddresses())
        , bv(bv)
        , newAccounts(endNewAccountId - beginNewAccountId)
        , height(height)
    { // OK
    }

    void register_reward(AccountId to, Funds amount, uint16_t offset) // OK
    {
        if (!validAccountId(to))
            throw Error(EIDPOLICY);
        if (amount.overflow())
            throw Error(EBALANCE);
        payouts.emplace_back(to, amount, height, offset);
        size_t i = payouts.size() - 1;
        auto& ref = payouts[i];
        auto& refTo = account_flow(to);
        if (to >= beginNewAccountId) {
            ref.toAddress = get_new_address(to - beginNewAccountId);
        } // otherwise wait for db lookup later
        refTo._in += amount;
        if (refTo._in.overflow())
            throw Error(EBALANCE);
        refTo.referredPayout.push_back(i);
    }

    void register_transfer(TransferView tv) // OK
    {
        Funds amount { tv.amount() };
        auto compactFee = tv.compact_fee();
        Funds fee { compactFee.uncompact() };
        AccountId to = tv.toAccountId();
        AccountId from = tv.fromAccountId();
        if (from == to)
            throw Error(ESELFSEND);
        if (amount.overflow() || fee.overflow())
            throw Error(EBALANCE);
        if (!validAccountId(from))
            throw Error(EIDPOLICY);
        if (!validAccountId(to))
            throw Error(EIDPOLICY);

        payments.emplace_back(from, compactFee, to, amount, tv.pin_nonce(), tv.signature());
        int i = payments.size() - 1;
        auto& ref = payments.back();
        if (from >= beginNewAccountId) {
            ref.fromAddress = get_new_address(from - beginNewAccountId);
        } // otherwise wait for db lookup later
        if (to >= beginNewAccountId) {
            ref.toAddress = get_new_address(to - beginNewAccountId);
        } // otherwise wait for db lookup later
        auto& refTo = account_flow(to);
        refTo._in += amount;
        if (refTo._in.overflow())
            throw Error(EBALANCE);
        refTo.referredTo.push_back(i);

        auto& refFrom = account_flow(from);
        refFrom._out += amount + fee;
        if (refFrom._out.overflow())
            throw Error(EBALANCE);
        totalfee += fee;
        if (totalfee.overflow())
            throw Error(EBALANCE);
        refFrom.referredFrom.push_back(i);
    }
    Funds getTotalFee() { return totalfee; }; // OK
    bool validAccountId(AccountId accountId) // OK
    {
        return accountId < endNewAccountId;
    }
    int set_address(OldAccountFlow& af, AddressView address) // OK
    {
        af.address = address;
        for (size_t i : af.referredFrom) {
            payments[i].fromAddress = af.address;
        }
        for (size_t i : af.referredTo) {
            payments[i].toAddress = af.address;
        }
        for (size_t i : af.referredPayout) {
            payouts[i].toAddress = af.address;
        }
        return 0;
    }
    auto& getOldAccounts() { return oldAccounts; } // OK
    const std::vector<AccountFlow>& get_new_accounts() const { return newAccounts; } // OK
    AccountId get_account_id(size_t newElementOffset) // OK
    {
        assert(newElementOffset < newAccounts.size());
        return beginNewAccountId + newElementOffset;
    };
    AddressView get_new_address(size_t i) { return bv.get_address(i); } // OK
    const std::vector<TransferInternal>& get_transfers() { return payments; };
    const std::vector<RewardInternal>& get_rewards() { return payouts; };

protected:
    AccountFlow& account_flow(AccountId i)
    {
        if (i < beginNewAccountId) {
            return oldAccounts[i];
        } else {
            assert(i < endNewAccountId);
            return newAccounts[i.value() - beginNewAccountId.value()];
        }
    }

private:
    Funds totalfee { 0 };
    AccountId beginNewAccountId;
    AccountId endNewAccountId;
    const BodyView& bv;
    std::map<AccountId, OldAccountFlow> oldAccounts;
    std::vector<AccountFlow> newAccounts;
    NonzeroHeight height;
    std::vector<RewardInternal> payouts;
    std::vector<TransferInternal> payments;
};

struct InsertHistoryEntry {
    InsertHistoryEntry(const RewardInternal& p, uint64_t historyId)
        : he(p)
        , historyId(historyId) {}
    InsertHistoryEntry(const VerifiedTransfer& t, uint64_t historyId)
        : he(t)
        , historyId(historyId) {}
    history::Entry he;
    uint64_t historyId;
};

struct HistoryEntries {
    HistoryEntries(uint64_t nextHistoryId)
        : nextHistoryId(nextHistoryId) {}
    uint64_t nextHistoryId;
    void push_reward(const RewardInternal& r)
    {
        insertHistory.emplace_back(r, nextHistoryId);
        insertAccountHistry.emplace_back(r.toAccountId, nextHistoryId);
        ++nextHistoryId;
    }
    void push_transfer(const VerifiedTransfer& r)
    {
        insertHistory.emplace_back(r, nextHistoryId);
        insertAccountHistry.emplace_back(r.ti.toAccountId, nextHistoryId);
        if (r.ti.toAccountId != r.ti.fromAccountId) {
            insertAccountHistry.emplace_back(r.ti.fromAccountId, nextHistoryId);
        }
        ++nextHistoryId;
    }
    void write(ChainDB& db)
    {
        // insert history for payouts and payments
        for (auto& p : insertHistory) {
            uint64_t inserted = db.insertHistory(p.he.hash, p.he.data);
            assert(p.historyId == inserted);
        }
        // insert account history
        for (auto p : insertAccountHistry) {
            db.insertAccountHistory(p.first, p.second);
        }
    }
    std::vector<InsertHistoryEntry> insertHistory;
    std::vector<std::pair<AccountId, uint64_t>> insertAccountHistry;
};

} // namespace

namespace chainserver {
struct Preparation {
    std::set<TransactionId> txset;
    std::vector<std::pair<AccountId, Funds>> updateBalances;
    std::vector<std::tuple<AddressView, Funds, AccountId>> insertBalances;
    HistoryEntries historyEntries;
    RollbackGenerator rg;
    Preparation(uint64_t nextHistoryId, AccountId beginNewAccountId)
        : historyEntries(nextHistoryId)
        , rg(beginNewAccountId)
    {
    }
};

Preparation BlockApplier::Preparer::prepare(const BodyView& bv, const NonzeroHeight height) const
{
    if (!bv.valid())
        throw Error(EMALFORMED);
    // Things to do in this function
    // * check corrupted data (avoid read overflow) OK
    // * sum up payouts OK
    // * no one can spend what they don't have OK
    // * overflow check OK
    // * check every new address is indeed new OK
    // * check signatures OK

    // Read new address section
    const AccountId beginNewAccountId = db.next_state_id(); // they start from this index
    Preparation res(db.next_history_id(), beginNewAccountId);
    BalanceChecker balanceChecker(beginNewAccountId, bv, height);

    { // verify address policy
        std::set<AddressView> newAddresses;
        // Check uniqueness of new addresses
        for (auto address : bv.addresses()) {
            if (newAddresses.emplace(address).second == false)
                throw Error(EADDRPOLICY);
            if (db.lookup_address(address))
                throw Error(EADDRPOLICY);
        }
    }

    // Read reward section
    Funds totalpayout { 0 };
    for (auto r: bv.rewards()){
        Funds amount { r.amount() };
        balanceChecker.register_reward(r.account_id(), amount, r.offset);
        totalpayout += amount;
        if (totalpayout.overflow())
            throw Error(EBALANCE);
    }

    // Read transfer section
    for (auto t : bv.transfers()) 
        balanceChecker.register_transfer(t);

    if (totalpayout > height.reward() + balanceChecker.getTotalFee())
        throw Error(EBALANCE);

    // loop through old accounts and
    // load previous balances and addresses from database
    auto& oldAccounts = balanceChecker.getOldAccounts();
    for (auto& [id, accountflow] : oldAccounts) {
        if (auto p = db.lookup_account(id); p) {
            // account lookup successful
            
            auto& [address, balance] = *p;
            res.rg.register_balance(id, p->funds);
            balanceChecker.set_address(accountflow, address);

            // check that balances are correct
            if (accountflow.out() > accountflow.in() + balance)
                throw Error(EBALANCE); // insufficient balance
            Funds newbalance { accountflow.in() + balance - accountflow.out() };
            res.updateBalances.push_back(std::make_pair(id, newbalance));
        } else {
            throw Error(EINVACCOUNT); // invalid account id (not found in database)
        }
    }

    // loop through new accounts
    auto& newAccounts = balanceChecker.get_new_accounts();
    for (size_t i = 0; i < newAccounts.size(); ++i) {
        auto& acc = newAccounts[i];
        if (acc.in().is_zero()) {
            throw Error(EIDPOLICY); // id was not referred
        }
        if (acc.out() > Funds(0)) // Not (acc.out() > acc.in()) because we do not like chains of new accounts
            throw Error(EBALANCE); // insufficient balance
        Funds balance = acc.in() - acc.out();
        AddressView address = balanceChecker.get_new_address(i);
        AccountId accountId = balanceChecker.get_account_id(i);
        res.insertBalances.emplace_back(address, balance, accountId);
    }

    // generate history for payments and check signatures
    // and check for unique transaction ids

    for (auto& r : balanceChecker.get_rewards()) {
        assert(!r.toAddress.is_null());
        res.historyEntries.push_reward(r);
    }
    for (auto& tr : balanceChecker.get_transfers()) {
        auto verified { tr.verify(hc, height) };
        TransactionId tid { verified.id };

        // check for duplicate txid (also within current block)
        if (baseTxIds.contains(tid) || newTxIds.contains(tid) || res.txset.emplace(tid).second == false) {
            throw Error(ENONCE);
        }

        res.historyEntries.push_transfer(verified);
    }
    return res;
}

void BlockApplier::apply_block(const BodyView& bv, NonzeroHeight height, BlockId blockId)
{
    auto prepared { preparer.prepare(bv, height) }; // call const function

    // ABOVE NO DB MODIFICATIONS
    //////////////////////////////
    // FOR EXCEPTION SAFETY //////
    // (ATOMICITY )         //////
    //////////////////////////////
    // BELOW NO "Error" TROWS

    try {
        preparer.newTxIds.merge(std::move(prepared.txset));

        // update old balances
        for (auto& [accId, bal] : prepared.updateBalances)
            db.set_balance(accId, bal);

        // insert new balances
        for (auto& [addr, bal, accId] : prepared.insertBalances)
            db.insertStateEntry(addr, bal, accId);

        // write undo data
        db.set_block_undo(blockId, prepared.rg.serialze());

        // write consensus data
        db.insert_consensus(height, blockId, db.next_history_id(), prepared.rg.begin_new_accounts());

        prepared.historyEntries.write(db);
    } catch (Error e) {
        throw std::runtime_error(std::string("Unexpected exception: ") + __PRETTY_FUNCTION__ + ":" + e.strerror());
    }
}
}
