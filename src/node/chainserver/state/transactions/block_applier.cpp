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
        std::vector<size_t> referredPayout;
        std::vector<size_t> referredTokenCreator;
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

    void register_reward(AccountId unvalidatedTo, Funds amount, uint16_t offset) // OK
    {
        auto to { validate_id(unvalidatedTo) };
        payouts.emplace_back(to, amount, height, offset);
        size_t i = payouts.size() - 1;
        auto& ref = payouts[i];
        auto& refTo = account_flow(to);
        if (to >= beginNewAccountId) {
            ref.toAddress = get_new_address(to - beginNewAccountId);
        } // otherwise wait for db lookup later
        refTo._in.add_throw(amount);
        refTo.referredPayout.push_back(i);
    }

    void register_transfer(TransferView tv, Height height) // OK
    {
        Funds amount { tv.amount_throw() };
        auto compactFee = tv.compact_fee_trow();
        Funds fee { compactFee.uncompact() };
        auto to { validate_id(tv.toAccountId()) };
        auto from { validate_id(tv.fromAccountId()) };
        if (height.value() > 719118 && amount.is_zero())
            throw Error(EZEROAMOUNT);
        if (from == to)
            throw Error(ESELFSEND);

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
    void register_token_creation(TokenCreationView tc, Height)
    {
        auto compactFee = tc.compact_fee_trow();
        Funds fee { compactFee.uncompact() };
        auto from { validate_id(tc.fromAccountId()) };

        tokenCreations.emplace_back(from, tc.pin_nonce(), tc.token_name(), compactFee, tc.signature());
        size_t i = tokenCreations.size() - 1;
        if (from >= beginNewAccountId) {
            auto& ref = tokenCreations.back();
            ref.creatorAddress = get_new_address(from - beginNewAccountId);
        } // otherwise wait for db lookup later

        auto& refFrom = account_flow(from);
        refFrom._out.add_throw(fee);
        totalfee.add_throw(fee);
        refFrom.referredTokenCreator.push_back(i);
    }
    Funds getTotalFee() { return totalfee; }; // OK
    ValidAccountId validate_id(AccountId accountId) const // OK
    {
        return accountId.validate_throw(endNewAccountId);
    }
    int set_address(OldAccountFlow& af, AddressView address) // OK
    {
        af.address = address;
        for (size_t i : af.referredPayout)
            payouts[i].toAddress = af.address;
        for (size_t i : af.referredTokenCreator)
            tokenCreations[i].creatorAddress = af.address;
        for (size_t i : af.referredFrom)
            payments[i].fromAddress = af.address;
        for (size_t i : af.referredTo)
            payments[i].toAddress = af.address;
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
    auto& get_transfers() const { return payments; }
    auto& get_rewards() const { return payouts; }
    auto& get_token_creations() const { return tokenCreations; }

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
    std::vector<RewardInternal> payouts;
    std::vector<TransferInternal> payments;
    std::vector<TokenCreationInternal> tokenCreations;
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
    InsertHistoryEntry(const VerifiedTokenCreation& t, HistoryId historyId)
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
    std::vector<std::tuple<TokenId, AccountId, TokenName>> insertTokenCreations;
    std::vector<API::Block::Reward> apiRewards;
    std::vector<API::Block::Transfer> apiTransfers;
    std::vector<API::Block::TokenCreation> apiTokenCreations;
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

    ////////////////////////////////////////////////////////////
    /// Read block sections
    ////////////////////////////////////////////////////////////

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
    Funds totalpayout { Funds::zero() };
    {
        auto r { bv.reward() };
        Funds amount { r.amount_throw() };
        balanceChecker.register_reward(r.account_id(), amount, r.offset);
        totalpayout.add_throw(amount);
    }

    // Read transfer section
    for (auto t : bv.transfers()) {
        balanceChecker.register_transfer(t, height);
    }

    // Read token creation section
    for (auto tc : bv.token_creations()) {
        balanceChecker.register_token_creation(tc, height);
    }

    ////////////////////////////////////////////////////////////
    /// Process block sections
    ////////////////////////////////////////////////////////////

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
            throw Error(EIDPOLICY); // id was not referred
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

    for (auto& r : balanceChecker.get_rewards()) {
        assert(!r.toAddress.is_null());
        auto& ref { res.historyEntries.push_reward(r) };

        res.apiRewards.push_back({
            .txhash { ref.he.hash },
            .toAddress { r.toAddress },
            .amount { r.amount },
        });
    }
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

    const auto beginNewTokenId = db.next_token_id(); // they start from this index
    for (size_t i = 0; i < balanceChecker.get_token_creations().size(); ++i) {
        auto& tc { balanceChecker.get_token_creations()[i] };
        auto tokenId { beginNewTokenId + i };
        const auto verified { tc.verify(hc, height, tokenId) };
        res.insertTokenCreations.push_back({ tokenId, tc.creatorAccountId, tc.tokenName });
        res.apiTokenCreations.push_back(
            {
                .creatorAddress { verified.tci.creatorAddress },
                .nonceId { verified.tci.pinNonce.id },
                .txhash { verified.hash },
                .tokenName { tc.tokenName },
                .tokenIndex { verified.tokenIndex },
                .fee { tc.compactFee.uncompact() },
            });
    }

    if (totalpayout > Funds::sum_throw(height.reward(), balanceChecker.getTotalFee()))
        throw Error(EBALANCE);

    return res;
}

API::Block BlockApplier::apply_block(const BodyView& bv, HeaderView hv, NonzeroHeight height, BlockId blockId)
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
            db.insert_state_entry(addr, bal, accId);
            balanceUpdates.insert_or_assign(accId, bal);
        }

        // insert token creations
        for (auto& [tokenId, creatorId, tokenName] : prepared.insertTokenCreations){
            db.insert_new_token(tokenId, height, creatorId, tokenName, TokenMintType::Default);
            db.insert_token_balance(tokenId, creatorId, DefaultTokenSupply);
        }

        // write undo data
        db.set_block_undo(blockId, prepared.rg.serialze());

        // write consensus data
        db.insert_consensus(height, blockId, db.next_history_id(), prepared.rg.begin_new_accounts());

        prepared.historyEntries.write(db);
        API::Block b(hv, height, 0);
        b.rewards = std::move(prepared.apiRewards);
        b.transfers = std::move(prepared.apiTransfers);
        return b;
    } catch (Error e) {
        throw std::runtime_error(std::string("Unexpected exception: ") + __PRETTY_FUNCTION__ + ":" + e.strerror());
    }
}
}
