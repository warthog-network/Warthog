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

struct TokenSectionInternal {
    TokenId id;
    std::vector<TokenTransferInternal> transfers;
    TokenSectionInternal(TokenId id)
        : id(id)
    {
    }
};

class BalanceChecker {

    class FundFlow {

    public:
        auto& in() const { return _in; }
        auto& out() const { return _out; }
        void add_in(Funds_uint64 f) { _in.add_throw(f); }
        void add_out(Funds_uint64 f) { _out.add_throw(f); }

    private:
        Funds_uint64 _in { Funds_uint64::zero() };
        Funds_uint64 _out { Funds_uint64::zero() };
    };

    using TokenFlow = std::map<TokenId, FundFlow>;
    struct AccountData {
    private:
        TokenFlow flow;

    public:
        void add(TokenId id, Funds_uint64 v)
        {
            flow[id].add_in(v);
        }
        void subtract(TokenId id, Funds_uint64 v)
        {
            flow[id].add_out(v);
        }
        auto& token_flow() const { return flow; }
        AddressView address;
        ValidAccountId id;
        AccountData(AddressView address, ValidAccountId accountId)
            : address(address)
            , id(accountId)
        {
        }
    };

    class OldAccountData {
    public:
        OldAccountData(ValidAccountId accountId)
            : address(Address::uninitialized())
            , data(address, accountId)
        {
        }
        auto& token_flow() { return data.token_flow(); }
        Address address;
        operator AccountData&() { return data; }

    private:
        AccountData data;
    };

    struct Accounts {
        Accounts(AccountId nextAccountId, const BodyView& bv)
            : beginNew(nextAccountId)
            , end(beginNew + bv.getNAddresses())
        {
            size_t n { end.value() - beginNew.value() };
            newAccounts.reserve(n);
            for (size_t i = 0; i < n; ++i)
                newAccounts.push_back({ bv.get_address(i), (beginNew + i).validate_throw(end) });
        }
        [[nodiscard]] AccountData& operator[](AccountId i)
        {
            auto vid { i.validate_throw(end) };
            if (i < beginNew) {
                return oldAccounts.try_emplace(vid, vid).first->second;
            } else {
                assert(i < end);
                return newAccounts[vid.value() - beginNew.value()];
            }
        }
        auto& new_accounts() const { return newAccounts; }
        auto& old_accounts() { return oldAccounts; }
        const AccountId beginNew;
        const AccountId end;

    private:
        std::vector<AccountData> newAccounts;
        std::map<ValidAccountId, OldAccountData> oldAccounts;
    };

protected:
    struct RewardArgument {
        AccountId to;
        Wart amount;
        RewardArgument(AccountId to, Wart amount)
            : to(std::move(to))
            , amount(std::move(amount))
        {
        }
    };

public:
    static RewardInternal register_reward(const BodyView& bv, Accounts& accounts, NonzeroHeight h)
    {
        auto r { bv.reward() };
        auto& a { accounts[r.account_id()] };
        auto am { r.amount_throw() };
        a.add(TokenId::WART, am);
        return {
            .toAccountId { a.id },
            .amount { am },
            .height { h },
            .toAddress { a.address }
        };
    }
    BalanceChecker(AccountId nextAccountId, const BodyView& bv, NonzeroHeight height)
        : height(height)
        , bv(bv)
        , accounts(nextAccountId, bv)
        , reward(register_reward(bv, accounts, height))
    {
    }

    void register_swap(AccountId accId, TokenFunds subtract, TokenFunds add)
    {
        auto& ad { accounts[accId] };
        ad.subtract(subtract.tokenId, subtract.funds);
        ad.add(add.tokenId, add.funds);
    }

    void charge_fee(AccountData& a, CompactUInt compactFee)
    {
        auto fee { compactFee.uncompact() };
        totalfee.add_throw(fee);
        a.subtract(TokenId::WART, fee);
    }

    TransferInternalWithoutAmount __register_transfer(TokenTransferView tv) // OK
    {
        auto tokenId { tv.token_id() };
        auto amount { tv.amount_throw() };
        auto compactFee { tv.compact_fee_throw() };
        auto& to { accounts[tv.toAccountId()] };
        auto& from { accounts[tv.fromAccountId()] };
        if (height.value() > 719118 && amount.is_zero())
            throw Error(EZEROAMOUNT);
        if (from.id == to.id)
            throw Error(ESELFSEND);

        to.add(tokenId, amount);
        from.subtract(tokenId, amount);
        charge_fee(from, compactFee);

        return {
            .from { from.id, from.address, tv.signature(), tv.pin_nonce() },
            .to { to.id, to.address },
            .compactFee { compactFee }
        };
    }
    void register_wart_transfer(WartTransferView tv)
    {
        wartTransfers.push_back({ __register_transfer(tv), tv.amount_throw() });
    }
    void register_token_section(BodyStructure::TokenSectionView v)
    {
        TokenSectionInternal td(v.id());
        v.foreach_transfer([&](TokenTransferView v) {
            td.transfers.push_back({ __register_transfer(v), v.amount_throw() });
        });
        v.foreach_order([&](OrderView v) { });
        v.foreach_liquidity_add([&](LiquidityAddView v) { });
        v.foreach_liquidity_remove([&](LiquidityRemoveView v) { });
        tokenSections.push_back(std::move(td));
    }

    void register_token_creation(TokenCreationView tc, Height)
    {
        auto compactFee = tc.compact_fee_throw();
        auto& from { accounts[tc.fromAccountId()] };

        tokenCreations.push_back({
            .creatorAccountId { from.id },
            .pinNonce { tc.pin_nonce() },
            .tokenName { tc.token_name() },
            .compactFee { compactFee },
            .signature { tc.signature() },
            .creatorAddress { from.address },
        });

        charge_fee(from, compactFee);
    }

    Wart getTotalFee() const { return totalfee; }; // OK
    auto& old_accounts() { return accounts.old_accounts(); }
    auto& get_new_accounts() const { return accounts.new_accounts(); }
    AddressView get_new_address(size_t i) { return bv.get_address(i); } // OK
    auto& token_creations() const { return tokenCreations; }
    const std::vector<WartTransferInternal>& get_wart_transfers() const { return wartTransfers; };
    const auto& get_reward() const { return reward; };

private:
    const NonzeroHeight height;
    const BodyView& bv;
    Accounts accounts;
    RewardInternal reward;

    std::vector<WartTransferInternal> wartTransfers;
    std::vector<TokenSectionInternal> tokenSections;
    // std::vector<TransferInternal> transfers;
    std::vector<TokenCreationInternal> tokenCreations;
    Wart totalfee { Wart::zero() };
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
    InsertHistoryEntry(const VerifiedWartTransfer& t, HistoryId historyId)
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
    [[nodiscard]] auto& push_transfer(const VerifiedWartTransfer& r)
    {
        auto& e { insertHistory.emplace_back(r, nextHistoryId) };
        insertAccountHistory.emplace_back(r.ti.to.id, nextHistoryId);
        if (r.ti.from.id != r.ti.to.id)
            insertAccountHistory.emplace_back(r.ti.from.id, nextHistoryId);
        ++nextHistoryId;
        return e;
    }
    [[nodiscard]] auto& push_token_transfer(const VerifiedTokenTransfer& r, TokenId tokenId)
    {
        auto& e { insertHistory.emplace_back(r, tokenId, nextHistoryId) };
        insertAccountHistory.emplace_back(r.ti.to.id, nextHistoryId);
        if (r.ti.to.id != r.ti.from.id)
            insertAccountHistory.emplace_back(r.ti.from.id, nextHistoryId);
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
class PreparetionGenerator;
class Preparation {
public:
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

private:
    friend class PreparationGenerator;
    Preparation(const ChainDB& db)
        : historyEntries(db.next_history_id())
        , rg(db)
    {
    }
};

class PreparationGenerator : public Preparation {
private:
    // constants
    const ChainDB& db;
    const Headerchain& hc;
    decltype(BlockApplier::Preparer::baseTxIds)& baseTxIds;
    const decltype(BlockApplier::Preparer::newTxIds)& newTxIds;
    const BodyView bv;
    const NonzeroHeight height;

    // variables needed for block verification
    const RewardView reward;
    BalanceChecker balanceChecker;

private:
    // Check uniqueness of new addresses
    void verify_new_address_policy()
    {
        std::set<AddressView> newAddresses;
        for (auto address : bv.addresses()) {
            if (newAddresses.emplace(address).second == false)
                throw Error(EADDRPOLICY);
            if (db.lookup_address(address))
                throw Error(EADDRPOLICY);
        }
    }

    void register_wart_transfers()
    {
        // Read transfer section for WART coins
        for (auto t : bv.wart_transfers())
            balanceChecker.register_wart_transfer(t);
    }
    void register_token_sections()
    {
        bv.foreach_token([&](BodyStructure::TokenSectionView t) {
            balanceChecker.register_token_section(t);
        });
    }

    void register_token_creations()
    {
        for (auto tc : bv.token_creations())
            balanceChecker.register_token_creation(tc, height);
    }

    auto process_new_flow(const AccountToken& at, const auto& tokenFlow)
    {
        if (tokenFlow.out() > Funds_uint64::zero()) // We do not allow resend of newly inserted balance
            throw Error(EBALANCE); // insufficient balance
        Funds_uint64 balance { tokenFlow.in() };
        insertBalances.push_back({ at, balance });
    }
    auto process_old_flow(const AccountToken& at, const auto& tokenFlow, const std::pair<BalanceId, Funds_uint64>& b)
    {
        const auto& [balanceId, balance] { b };
        rg.register_balance(balanceId, balance);
        // check that balances are correct
        auto totalIn { Funds_uint64::sum_throw(tokenFlow.in(), balance) };
        Funds_uint64 newbalance { Funds_uint64::diff_throw(totalIn, tokenFlow.out()) };
        updateBalances.push_back({ balanceId, at, newbalance });
    }
    auto db_address(AccountId id)
    {
        if (auto address { db.lookup_address(id) })
            return *address;
        throw Error(EINVACCOUNT); // invalid account id (not found in database)
    }

    void process_old_accounts()
    {
        for (auto& [accountId, accountData] : balanceChecker.old_accounts()) {
            accountData.address = db_address(accountId);
            for (auto& [tokenId, tokenFlow] : accountData.token_flow()) {
                AccountToken at { accountId, tokenId };
                if (auto p { db.get_balance(at) })
                    process_old_flow(at, tokenFlow, *p);
                else
                    process_new_flow(at, tokenFlow);
            }
        }
    }

    void process_new_accounts()
    {
        // loop through new accounts
        for (auto& a : balanceChecker.get_new_accounts()) {
            bool referred { false };
            for (auto& [tokenId, tokenFlow] : a.token_flow()) {
                if (!tokenFlow.in().is_zero())
                    referred = true;
                process_new_flow({ a.id, tokenId }, tokenFlow);
            }
            if (!referred)
                throw Error(EIDPOLICY); // id was not referred
            insertAccounts.push_back({ a.address, a.id });
        }
    }

    void process_reward()
    {
        const auto& balanceChecker { this->balanceChecker }; // shadow balanceChecker

        auto& r { balanceChecker.get_reward() };
        if (r.amount > Wart::sum_throw(height.reward(), balanceChecker.getTotalFee()))
            throw Error(EBALANCE);
        assert(!r.toAddress.is_null());
        auto& ref { historyEntries.push_reward(r) };
        apiReward = api::Block::Reward {
            .txhash { ref.he.hash },
            .toAddress { r.toAddress },
            .amount { r.amount },
        };
    }

    auto tx_verifier()
    {
        return TransactionVerifier {
            hc, height,
            std::function<bool(TransactionId)>(
                [this](TransactionId tid) -> bool {
                    // check for duplicate txid (also within current block)
                    return !baseTxIds.contains(tid) && !newTxIds.contains(tid) && txset.emplace(tid).second;
                })
        };
    }

    // generate history for transfers and check signatures
    // and check for unique transaction ids
    void process_wart_transfers()
    {
        const auto& balanceChecker { this->balanceChecker }; // shadow balanceChecker
        for (auto& tr : balanceChecker.get_wart_transfers()) {
            auto verified { tr.verify(tx_verifier()) };

            auto& ref { historyEntries.push_transfer(verified) };
            apiTransfers.push_back(api::Block::Transfer {
                .fromAddress { tr.from.address },
                .fee { tr.compactFee.uncompact() },
                .nonceId { tr.from.pinNonce.id },
                .pinHeight { verified.txid.pinHeight },
                .txhash { ref.he.hash },
                .toAddress { tr.to.address },
                .amount { tr.amount },
            });
        }
    }

public:
    // Things to do in this constructor
    // * sum up payouts OK
    // * no one can spend what they don't have OK
    // * overflow check OK
    // * check every new address is indeed new OK
    // * check signatures OK
    PreparationGenerator(const BlockApplier::Preparer& preparer, const ParsedBlock& b)
        : Preparation(preparer.db)
        , db(preparer.db)
        , hc(preparer.hc)
        , baseTxIds(preparer.baseTxIds)
        , newTxIds(preparer.newTxIds)
        , bv { b.body.view() }
        , height(b.height)
        , reward(bv.reward())
        , balanceChecker(db.next_account_id(), bv, height)
    {

        /// Read block sections
        verify_new_address_policy(); // new address section
        register_wart_transfers(); // WART transfer section
        register_token_sections();
        register_token_creations(); // token creation section

        /// Process block sections
        process_old_accounts();
        process_new_accounts();
        process_reward();
        process_wart_transfers();

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
    }
};

Preparation BlockApplier::Preparer::prepare(const ParsedBlock& b) const
{
    return PreparationGenerator(*this, b);
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
