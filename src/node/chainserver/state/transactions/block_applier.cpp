#include "block_applier.hpp"
#include "api/types/all.hpp"
#include "block/body/parse.hpp"
#include "block/body/rollback.hpp"
#include "block/chain/header_chain.hpp"
#include "block/chain/history/history.hpp"
#include "chainserver/db/chain_db.hpp"
#include "defi/uint64/lazy_matching.hpp"
#include "defi/uint64/pool.hpp"

namespace {

struct OrderAggregator {
    OrderAggregator(OrderLoader l)
        : l(std::move(l))
    {
        load_next();
    }
    defi::Order_uint64 load_next_order()
    {
        assert(!finished);
        auto o { last_inserted().order };
        while (true) {
            load_next();
            if (finished || last_inserted().order.limit != o.limit)
                break;
            o.amount.add_assert(last_inserted().remaining());
        }
        return o;
    }
    std::optional<Price_uint64> next_price() const
    {
        if (finished)
            return {};
        return last_inserted().order.limit;
    }
    auto& orders() const { return loaded; }

private:
    const OrderData& last_inserted() const
    {
        return loaded.back();
    }
    void load_next()
    {
        if (finished == true)
            return;

        auto o { l() };
        if (!o) {
            finished = true;
            return;
        }
        loaded.push_back(*o);
    }

private:
    bool finished { false };
    std::vector<OrderData> loaded;
    OrderLoader l;
};

void match(const ChainDB& db, TokenId tid, defi::PoolLiquidity_uint64 p)
{
    OrderAggregator baseSellAggregator { db.base_order_loader(tid) };
    OrderAggregator quoteBuyAggregator { db.quote_order_loader(tid) };
    const auto m { defi::match_lazy(baseSellAggregator, quoteBuyAggregator, p) };

    auto returned { m.filled };
    Funds_uint64 fromPool { 0 };
    if (m.toPool) {
        auto pa { m.toPool->amount };
        if (m.toPool->isQuote) {
            returned.quote.subtract_assert(pa);
            fromPool = p.buy(pa, 50);
            returned.base.add_assert(fromPool);
        } else {
            returned.base.subtract_assert(pa);
            fromPool = p.sell(pa, 50);
            returned.quote.add_assert(fromPool);
        }
    }
    { // seller match
        Funds_uint64 quoteDistributed { 0 };
        auto remaining { m.filled.base };
        for (auto& o : baseSellAggregator.orders()) {
            if (remaining == 0)
                break;
            auto b { std::min(remaining, o.remaining()) };
            remaining.subtract_assert(b);

            // compute return of that order
            auto q { Prod128(b, returned.quote).divide_floor(m.filled.base.value()) };
            assert(q.has_value());
            quoteDistributed.add_assert(*q);

            // order swapped b -> q
        }
        assert(remaining == 0);
        returned.quote.subtract_assert(quoteDistributed);
    }
    { // buyer match
        Funds_uint64 baseDistributed { 0 };
        auto remaining { m.filled.quote };
        for (auto& o : quoteBuyAggregator.orders()) {
            if (remaining == 0)
                break;
            auto q { std::min(remaining, o.remaining()) };
            remaining.subtract_assert(q);

            // compute return of that order
            auto b { Prod128(q, returned.base).divide_floor(m.filled.quote.value()) };
            assert(b.has_value());
            baseDistributed.add_assert(*b);

            // order swapped q -> b
        }
        assert(remaining == 0);
        returned.base.subtract_assert(baseDistributed);
    }
}

class BalanceChecker {

    class FundFlow {
        friend class BalanceChecker;

    public:
        Funds_uint64 in() const { return _in; }
        Funds_uint64 out() const { return _out; }

    private:
        Funds_uint64 _in { Funds_uint64::zero() };
        Funds_uint64 _out { Funds_uint64::zero() };
    };

    using TokenFlow = std::map<TokenId, FundFlow>;
    struct AccountData {
        bool isMiner { false };
        std::vector<size_t> referredTokenCreator;
        std::vector<size_t> referredFrom;
        std::vector<size_t> referredTo;
        TokenFlow tokenFlow;
    };

    class OldAccountData : public AccountData {
    public:
        OldAccountData(Address address)
            : address(std::move(address))
        {
        }
        Address address;
    };

protected:
    [[nodiscard]] AccountData& account_data(AccountId i)
    {
        if (i < beginNewAccountId) {
            return oldAccounts[i];
        } else {
            assert(i < endNewAccountId);
            return newAccounts[i.value() - beginNewAccountId.value()];
        }
    }
    auto& token_flow(AccountId aid, TokenId tid)
    {
        return account_data(aid).tokenFlow[tid];
    }

public:
    struct RewardArgument {
        AccountId to;
        Funds_uint64 amount;
        uint16_t offset;
        RewardArgument(AccountId to, Funds_uint64 amount, uint16_t offset)
            : to(std::move(to))
            , amount(std::move(amount))
            , offset(std::move(offset))
        {
        }
    };
    BalanceChecker(uint64_t nextStateId,
        const BodyView& bv, NonzeroHeight height, RewardArgument r)
        : beginNewAccountId(nextStateId)
        , endNewAccountId(beginNewAccountId + bv.getNAddresses())
        , bv(bv)
        , newAccounts(endNewAccountId - beginNewAccountId)
        , height(height)
        , reward(validate_id(r.to), r.amount, height, r.offset)
    { // OK
        if (r.to >= beginNewAccountId) {
            reward.toAddress = get_new_address(r.to - beginNewAccountId);
        } // otherwise wait for db lookup later
        auto& a = account_data(r.to);
        a.tokenFlow[TokenId::WART]._in.add_throw(r.amount);
        a.isMiner = true;
    }

    struct TokenBalance {
        TokenId tokenId;
        Funds_uint64 funds;
    };

    void register_swap(AccountId accId, TokenBalance subtract, TokenBalance add)
    {
        auto vid { validate_id(accId) };
        auto& ad { account_data(vid) };
        ad.tokenFlow[subtract.tokenId].out().add_throw(subtract.funds);
        ad.tokenFlow[add.tokenId].out().add_throw(add.funds);
    }

    void register_transfer(TokenId tokenId, WartTransferView tv, Height height) // OK
    {
        constexpr uint32_t fivedaysBlocks = 5 * 24 * 60 * 3;
        constexpr uint32_t unblockXeggexHeight = 2576442 + fivedaysBlocks;
        static_assert(2598042 == unblockXeggexHeight);
        if (tv.fromAccountId().value() == 1910 && (height.value() > 2534437) && (height.value() < unblockXeggexHeight)) {
            throw Error(EFROZENACC); // freeze Xeggex acc temporarily
        }
        Funds_uint64 amount { tv.amount_throw() };
        auto compactFee = tv.compact_fee_trow();
        Funds_uint64 fee { compactFee.uncompact() };
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

        { // destination balance
            auto& ad { account_data(to) };
            ad.referredTo.push_back(i);
            ad.tokenFlow[tokenId]._in.add_throw(amount);
        }
        { // source balance
            auto& ad { account_data(from) };
            ad.referredFrom.push_back(i);
            ad.tokenFlow[tokenId]._out.add_throw(amount);
            ad.tokenFlow[TokenId::WART]._out.add_throw(fee);
            totalfee.add_throw(fee);
        }
    }
    void register_token_creation(TokenCreationView tc, Height)
    {
        auto compactFee = tc.compact_fee_trow();
        Funds_uint64 fee { compactFee.uncompact() };
        auto from { validate_id(tc.fromAccountId()) };

        tokenCreations.emplace_back(from, tc.pin_nonce(), tc.token_name(), compactFee, tc.signature());
        size_t i = tokenCreations.size() - 1;
        if (from >= beginNewAccountId) {
            auto& ref = tokenCreations.back();
            ref.creatorAddress = get_new_address(from - beginNewAccountId);
        } // otherwise wait for db lookup later

        auto& ad = account_data(from);
        ad.tokenFlow[TokenId::WART]._out.add_throw(fee);
        totalfee.add_throw(fee);
        ad.referredTokenCreator.push_back(i);
    }

    Funds_uint64 getTotalFee() { return totalfee; }; // OK
    ValidAccountId validate_id(AccountId accountId) const // OK
    {
        return accountId.validate_throw(endNewAccountId);
    }
    int set_address(OldAccountData& af, AddressView address) // OK
    {
        af.address = address;
        for (size_t i : af.referredTokenCreator)
            tokenCreations[i].creatorAddress = af.address;
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
    auto& old_accounts() { return oldAccounts; } // OK
    const std::vector<AccountData>& get_new_accounts() const { return newAccounts; } // OK
    AccountId get_account_id(size_t newElementOffset) // OK
    {
        assert(newElementOffset < newAccounts.size());
        return beginNewAccountId + newElementOffset;
    };
    AddressView get_new_address(size_t i) { return bv.get_address(i); } // OK
    auto& token_creations() const { return tokenCreations; }
    const std::vector<TransferInternal>& get_transfers() { return payments; };
    const auto& get_reward() { return reward; };

private:
    Funds_uint64 totalfee { Funds_uint64::zero() };
    AccountId beginNewAccountId;
    AccountId endNewAccountId;
    const BodyView& bv;
    std::map<AccountId, OldAccountData> oldAccounts;
    std::vector<AccountData> newAccounts;
    NonzeroHeight height;
    RewardInternal reward;
    std::vector<TransferInternal> payments;

    std::vector<TokenCreationInternal> tokenCreations;
};

struct InsertHistoryEntry {
    InsertHistoryEntry(const RewardInternal& p, HistoryId historyId)
        : he(p)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(const VerifiedTokenTransfer& t, TokenId tokenId, HistoryId historyId)
        : he(t, tokenId)
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
    [[nodiscard]] auto& push_token_transfer(const VerifiedTokenTransfer& r, TokenId tokenId)
    {
        auto& e { insertHistory.emplace_back(r, tokenId, nextHistoryId) };
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
    HistoryEntries historyEntries;
    RollbackGenerator rg;
    std::set<TransactionId> txset;
    std::vector<std::tuple<BalanceId, AccountToken, Funds_uint64>> updateBalances;
    std::vector<std::tuple<AccountToken, Funds_uint64>> insertBalances;
    std::vector<std::tuple<AddressView, AccountId>> insertAccounts;
    std::vector<std::tuple<AccountToken, TokenName>> insertTokenCreations;
    std::optional<api::Block::Reward> apiReward;
    std::vector<api::Block::Transfer> apiTransfers;
    std::vector<api::Block::TokenCreation> apiTokenCreations;
    Preparation(const BlockApplier::Preparer& preparer, const ParsedBlock& b);
};

Preparation::Preparation(const BlockApplier::Preparer& preparer, const ParsedBlock& b)
    : historyEntries(preparer.db.next_history_id())
    , rg(preparer.db)
{
    // Things to do in this function
    // * check corrupted data (avoid read overflow) OK
    // * sum up payouts OK
    // * no one can spend what they don't have OK
    // * overflow check OK
    // * check every new address is indeed new OK
    // * check signatures OK

    // abbreviations
    const ChainDB& db { preparer.db };
    const Headerchain& hc { preparer.hc };
    auto& baseTxIds { preparer.baseTxIds };
    auto& newTxIds { preparer.newTxIds };
    auto bv { b.body.view() };
    auto height { b.height };

    ////////////////////////////////////////////////////////////
    /// Read block sections
    ////////////////////////////////////////////////////////////

    // Read new address section

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
    Funds_uint64 totalpayout { Funds_uint64::zero() };
    BalanceChecker balanceChecker {
        [&]() {
            auto r { bv.reward() };
            Funds_uint64 amount { r.amount_throw() };
            totalpayout.add_throw(amount);
            return BalanceChecker { db.next_state_id(), bv, height, { r.account_id(), amount, r.offset } };
        }()
    };

    // Read transfer section for WART coins
    for (auto t : bv.transfers()) {
        balanceChecker.register_transfer(TokenId(0), t, height);
    }

    // Read token creation section
    for (auto tc : bv.token_creations()) {
        balanceChecker.register_token_creation(tc, height);
    }

    ////////////////////////////////////////////////////////////
    /// Process block sections
    ////////////////////////////////////////////////////////////
    auto process_new_balance { [this](auto tokenId, const auto& tokenFlow, auto accountId) {
        if (tokenFlow.out() > Funds_uint64::zero()) // We do not allow resend of newly inserted balance
            throw Error(EBALANCE); // insufficient balance
        assert(tokenFlow.out().is_zero());
        Funds_uint64 balance = tokenFlow.in();
        insertBalances.push_back({ { accountId, tokenId }, balance });
    } };

    // loop through old accounts and
    // load previous balances and addresses from database
    for (auto& [accountId, accountData] : balanceChecker.old_accounts()) {
        if (auto address { db.lookup_address(accountId) }; address) {
            // address lookup successful
            balanceChecker.set_address(accountData, *address);

            for (auto& [tokenId, tokenFlow] : accountData.tokenFlow) {
                if (auto p { db.get_balance({ accountId, tokenId }) }) {
                    const auto& [balanceId, balance] { *p };
                    rg.register_balance(balanceId, balance);
                    // check that balances are correct
                    auto totalIn { Funds_uint64::sum_throw(tokenFlow.in(), balance) };
                    Funds_uint64 newbalance { Funds_uint64::diff_throw(totalIn, tokenFlow.out()) };
                    updateBalances.push_back({ balanceId, { accountId, tokenId }, newbalance });
                } else {
                    process_new_balance(tokenId, tokenFlow, accountId);
                }
            }
        } else {
            throw Error(EINVACCOUNT); // invalid account id (not found in database)
        }
    }

    // loop through new accounts
    const auto& newAccounts = balanceChecker.get_new_accounts();
    for (size_t i = 0; i < newAccounts.size(); ++i) {
        auto& acc = newAccounts[i];
        AccountId accountId = balanceChecker.get_account_id(i);
        bool referred { false };
        for (auto& [tokenId, tokenFlow] : acc.tokenFlow) {
            if (!tokenFlow.in().is_zero())
                referred = true;
            process_new_balance(tokenId, tokenFlow, accountId);
        }
        if (!referred) {
            throw Error(EIDPOLICY); // id was not referred
        }
        AddressView address = balanceChecker.get_new_address(i);
        insertAccounts.push_back({ address, accountId });
    }

    // generate history for payments and check signatures
    // and check for unique transaction ids

    { // reward
        auto& r { balanceChecker.get_reward() };
        assert(!r.toAddress.is_null());
        auto& ref { historyEntries.push_reward(r) };
        apiReward = api::Block::Reward {
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
        if (baseTxIds.contains(tid) || newTxIds.contains(tid) || txset.emplace(tid).second == false) {
            throw Error(ENONCE);
        }

        auto& ref { historyEntries.push_transfer(verified) };
        apiTransfers.push_back({
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
    for (size_t i = 0; i < balanceChecker.token_creations().size(); ++i) {
        auto& tc { balanceChecker.token_creations()[i] };
        auto tokenId { beginNewTokenId + i };
        const auto verified { tc.verify(hc, height, tokenId) };
        insertTokenCreations.push_back({ { tc.creatorAccountId, tokenId }, tc.tokenName });
        apiTokenCreations.push_back(
            {
                .creatorAddress { verified.tci.creatorAddress },
                .nonceId { verified.tci.pinNonce.id },
                .txhash { verified.hash },
                .tokenName { tc.tokenName },
                .tokenIndex { verified.tokenIndex },
                .fee { tc.compactFee.uncompact() },
            });
    }

    if (totalpayout > Funds_uint64::sum_throw(height.reward(), balanceChecker.getTotalFee()))
        throw Error(EBALANCE);
}

Preparation BlockApplier::Preparer::prepare(const ParsedBlock& b) const
{
    return Preparation(*this, b);
}

api::Block BlockApplier::apply_block(const ParsedBlock& b, BlockId blockId)
{
    auto prepared { preparer.prepare(b) }; // call const function

    // ABOVE NO DB MODIFICATIONS
    //////////////////////////////
    // FOR EXCEPTION SAFETY //////
    // (ATOMICITY )         //////
    //////////////////////////////
    // BELOW NO "Error" TROWS

    try {
        preparer.newTxIds.merge(std::move(prepared.txset));

        // create new pools
        // create new tokens

        // update old balances
        for (auto& [balId, accountToken, bal] : prepared.updateBalances) {
            db.set_balance(balId, bal);
            balanceUpdates.insert_or_assign(accountToken, bal);
        }

        // new accounts
        for (auto& [address, accId] : prepared.insertAccounts)
            db.insert_account(address, accId);

        // insert token creations
        for (auto& [at, tokenName] : prepared.insertTokenCreations)
            db.insert_new_token(at.token_id(), b.height, at.account_id(), tokenName, TokenMintType::Ownall);

        // insert new balances
        for (auto& [at, bal] : prepared.insertBalances) {
            db.insert_token_balance(at, bal);
            balanceUpdates.insert_or_assign(at, bal);
        }

        // write undo data
        db.set_block_undo(blockId, prepared.rg.serialze());

        // write consensus data
        db.insert_consensus(b.height, blockId, db.next_history_id(), prepared.rg.next_state_id());

        prepared.historyEntries.write(db);
        return api::Block(b.header, b.height, 0, std::move(prepared.apiReward), std::move(prepared.apiTransfers));
    } catch (Error e) {
        throw std::runtime_error(std::string("Unexpected exception: ") + __PRETTY_FUNCTION__ + ":" + e.strerror());
    }
}
}
