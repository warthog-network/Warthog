#include "block_applier.hpp"
#include "api/types/all.hpp"
#include "block/body/parse.hpp"
#include "block/body/rollback.hpp"
#include "block/chain/header_chain.hpp"
#include "block/chain/history/history.hpp"
#include "chainserver/db/chain_db.hpp"

namespace {

class BalanceChecker {

    class AccountFlow {
        friend class BalanceChecker;

    public:
        Funds in() const { return _in; }
        Funds out() const { return _out; }

    private:
        Funds _in { Funds::zero() };
        Funds _out { Funds::zero() };
        std::vector<size_t> referredFrom;
        std::vector<size_t> referredTo;
        bool isMiner { false };
    };

    class OldAccountFlow : public AccountFlow {
        friend class BalanceChecker;
        Address address;
    };

public:
    struct RewardArgument {
        AccountId to;
        Funds amount;
        uint16_t offset;
        RewardArgument(AccountId to, Funds amount, uint16_t offset)
            : to(std::move(to))
            , amount(std::move(amount))
            , offset(std::move(offset))
        {
        }
    };
    BalanceChecker(AccountId beginNewAccountId,
        const BodyView& bv, NonzeroHeight height, RewardArgument r)
        : beginNewAccountId(beginNewAccountId)
        , endNewAccountId(beginNewAccountId + bv.getNAddresses())
        , bv(bv)
        , newAccounts(endNewAccountId - beginNewAccountId)
        , height(height)
        , reward(r.to, r.amount, height, r.offset)
    { // OK
        if (!validAccountId(r.to))
            throw Error(EIDPOLICY);
        if (r.to >= beginNewAccountId) {
            reward.toAddress = get_new_address(r.to - beginNewAccountId);
        } // otherwise wait for db lookup later
        auto& refTo = account_flow(r.to);
        refTo._in.add_throw(r.amount);
        refTo.isMiner = true;
        ;
    }

    void register_transfer(TransferView tv, Height height) // OK
    {
        constexpr uint32_t fivedaysBlocks = 5 * 24 * 60 * 3;
        constexpr uint32_t unblockXeggexHeight = 2576442 + fivedaysBlocks;
        static_assert(2598042 == unblockXeggexHeight);
        if (tv.fromAccountId().value() == 1910 && (height.value() > 2534437) && (height.value() < unblockXeggexHeight)) {
            throw Error(EFROZENACC); // freeze Xeggex acc temporarily
        }
        Funds amount { tv.amount_throw() };
        auto compactFee = tv.compact_fee_trow();
        Funds fee { compactFee.uncompact() };
        AccountId to = tv.toAccountId();
        AccountId from = tv.fromAccountId();
        if (height.value() > 719118 && amount.is_zero())
            throw Error(EZEROAMOUNT);
        if (from == to)
            throw Error(ESELFSEND);
        if (!validAccountId(from))
            throw Error(EIDPOLICY);
        if (!validAccountId(to))
            throw Error(EIDPOLICY);

        payments.emplace_back(from, compactFee, to, amount, tv.pin_nonce(), tv.signature());
        size_t i = payments.size() - 1;
        auto& ref = payments.back();
        if (from >= beginNewAccountId) {
            ref.fromAddress = get_new_address(from - beginNewAccountId);
        } // otherwise wait for db lookup later
        if (to >= beginNewAccountId) {
            ref.toAddress = get_new_address(to - beginNewAccountId);
        } // otherwise wait for db lookup later
        auto& refTo = account_flow(to);
        refTo._in.add_throw(amount);
        refTo.referredTo.push_back(i);

        auto& refFrom = account_flow(from);
        refFrom._out.add_throw(Funds::sum_throw(amount, fee));
        totalfee.add_throw(fee);
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
        if (af.isMiner) {
            reward.toAddress = af.address;
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
    const auto& get_reward() { return reward; };

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
    Funds totalfee { Funds::zero() };
    AccountId beginNewAccountId;
    AccountId endNewAccountId;
    const BodyView& bv;
    std::map<AccountId, OldAccountFlow> oldAccounts;
    std::vector<AccountFlow> newAccounts;
    NonzeroHeight height;
    RewardInternal reward;
    std::vector<TransferInternal> payments;
};

struct InsertHistoryEntry {
    InsertHistoryEntry(const RewardInternal& p, HistoryId historyId)
        : he(p)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(const VerifiedTransfer& t, HistoryId historyId)
        : he(t)
        , historyId(historyId)
    {
    }
    history::Entry he;
    HistoryId historyId;
};

struct HistoryEntries {
    HistoryEntries(HistoryId nextHistoryId)
        : nextHistoryId(nextHistoryId)
    {
    }
    HistoryId nextHistoryId;
    [[nodiscard]] const auto& push_reward(const RewardInternal& r)
    {
        auto& e { insertHistory.emplace_back(r, nextHistoryId) };
        insertAccountHistory.emplace_back(r.toAccountId, nextHistoryId);
        ++nextHistoryId;
        return e;
    }
    [[nodiscard]] auto& push_transfer(const VerifiedTransfer& r)
    {
        auto& e { insertHistory.emplace_back(r, nextHistoryId) };
        insertAccountHistory.emplace_back(r.ti.toAccountId, nextHistoryId);
        if (r.ti.toAccountId != r.ti.fromAccountId) {
            insertAccountHistory.emplace_back(r.ti.fromAccountId, nextHistoryId);
        }
        ++nextHistoryId;
        return e;
    }
    void write(ChainDB& db)
    {
        // insert history for payouts and payments
        for (auto& p : insertHistory) {
            auto inserted { db.insertHistory(p.he.hash, p.he.data) };
            assert(p.historyId == inserted);
        }
        // insert account history
        for (auto p : insertAccountHistory) {
            db.insertAccountHistory(p.first, p.second);
        }
    }
    std::vector<InsertHistoryEntry> insertHistory;
    std::vector<std::pair<AccountId, HistoryId>> insertAccountHistory;
};

} // namespace

namespace chainserver {
struct Preparation {
    std::set<TransactionId> txset;
    std::vector<std::pair<AccountId, Funds>> updateBalances;
    std::vector<std::tuple<AddressView, Funds, AccountId>> insertBalances;
    std::optional<api::Block::Reward> apiReward;
    std::vector<api::Block::Transfer> apiTransfers;
    HistoryEntries historyEntries;
    RollbackGenerator rg;
    Preparation(HistoryId nextHistoryId, AccountId beginNewAccountId)
        : historyEntries(nextHistoryId)
        , rg(beginNewAccountId)
    {
    }
};

Preparation BlockApplier::Preparer::prepare(const BodyView& bv, const NonzeroHeight height) const
{
    if (!bv.valid())
        throw Error(EINV_BODY);
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
    Funds totalpayout { Funds::zero() };
    BalanceChecker balanceChecker {
        [&]() {
            auto r { bv.reward() };
            Funds amount { r.amount_throw() };
            totalpayout.add_throw(amount);
            return BalanceChecker { beginNewAccountId, bv, height, { r.account_id(), amount, r.offset } };
        }()
    };

    // Read transfer section
    for (auto t : bv.transfers()) {
        balanceChecker.register_transfer(t, height);
    }

    if (totalpayout > Funds::sum_throw(height.reward(), balanceChecker.getTotalFee()))
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
            auto totalIn { Funds::sum_throw(accountflow.in(), balance) };
            Funds newbalance { Funds::diff_throw(totalIn, accountflow.out()) };
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
            throw Error(EIDNOTREFERENCED); // id was not referred
        }
        if (acc.out() > Funds::zero()) // Not (acc.out() > acc.in()) because we do not like chains of new accounts
            throw Error(EBALANCE); // insufficient balance
        assert(acc.out().is_zero());
        Funds balance = acc.in();
        AddressView address = balanceChecker.get_new_address(i);
        AccountId accountId = balanceChecker.get_account_id(i);
        res.insertBalances.emplace_back(address, balance, accountId);
    }

    // generate history for payments and check signatures
    // and check for unique transaction ids

    { // reward
        auto& r { balanceChecker.get_reward() };
        assert(!r.toAddress.is_null());
        auto& ref { res.historyEntries.push_reward(r) };
        res.apiReward = api::Block::Reward {
            .txhash { ref.he.hash },
            .toAddress { r.toAddress },
            .amount { r.amount },
        };
    }

    // transfers
    for (auto& tr : balanceChecker.get_transfers()) {
        auto verified { tr.verify(hc, height) };
        TransactionId tid { verified.id };

        // check for duplicate txid (also within current block)
        if (baseTxIds.contains(tid) || newTxIds.contains(tid) || res.txset.emplace(tid).second == false) {
            throw Error(ENONCE);
        }

        auto& ref { res.historyEntries.push_transfer(verified) };
        res.apiTransfers.push_back({
            .fromAddress { tr.fromAddress },
            .fee { tr.compactFee.uncompact() },
            .nonceId { tr.pinNonce.id },
            .pinHeight { tr.pinNonce.pin_height(PinFloor { PrevHeight { height } }) },
            .txhash { ref.he.hash },
            .toAddress { tr.toAddress },
            .amount { tr.amount },
        });
    }
    return res;
}

api::Block BlockApplier::apply_block(const BodyView& bv, HeaderView hv, NonzeroHeight height, BlockId blockId)
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
        for (auto& [accId, bal] : prepared.updateBalances) {
            db.set_balance(accId, bal);
            balanceUpdates.insert_or_assign(accId, bal);
        }

        // insert new balances
        for (auto& [addr, bal, accId] : prepared.insertBalances) {
            db.insertStateEntry(addr, bal, accId);
            balanceUpdates.insert_or_assign(accId, bal);
        }

        // write undo data
        db.set_block_undo(blockId, prepared.rg.serialze());

        // write consensus data
        db.insert_consensus(height, blockId, db.next_history_id(), prepared.rg.begin_new_accounts());

        prepared.historyEntries.write(db);
        api::Block b(hv, height, 0, std::move(prepared.apiReward), std::move(prepared.apiTransfers));
        return b;
    } catch (Error e) {
        throw std::runtime_error(std::string("Unexpected exception: ") + __PRETTY_FUNCTION__ + ":" + e.strerror());
    }
}
}
