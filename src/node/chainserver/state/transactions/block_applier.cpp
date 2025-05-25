#include "block_applier.hpp"
#include "api/types/all.hpp"
#include "block/body/parse.hpp"
#include "block/body/rollback.hpp"
#include "block/chain/header_chain.hpp"
#include "block/chain/history/history.hpp"
#include "chainserver/db/chain_db.hpp"
#include "chainserver/db/ids.hpp"
#include "chainserver/db/types.hpp"
#include "defi/token/info.hpp"
#include "defi/uint64/lazy_matching.hpp"
#include "defi/uint64/pool.hpp"

using namespace block::body;

namespace {

struct VerifiedOrderWithId : public VerifiedOrder {
    VerifiedOrderWithId(VerifiedOrder vo, HistoryId orderId);
    HistoryId orderId;
};

template <bool ASCENDING>
using UnsortedOrders = std::vector<VerifiedOrderWithId>;
using UnsortedSells = UnsortedOrders<true>;
using UnsortedBuys = UnsortedOrders<false>;

struct UnsortedOrderbook {
    UnsortedSells sells;
    UnsortedBuys buys;
};

template <bool ASCENDING>
struct SortedOrders : public std::vector<OrderData> {
    static bool in_order(const OrderData& o1, const OrderData& o2)
    {
        if constexpr (ASCENDING) {
            if (o1.order.limit < o2.order.limit)
                return true;
            if (o1.order.limit > o2.order.limit)
                return false;
        } else {
            if (o1.order.limit > o2.order.limit)
                return true;
            if (o1.order.limit < o2.order.limit)
                return false;
        }
        return o1.id < o2.id; // first orders first at same price, id ascending.
                              // The same order that orders are fetched from db
                              // by OrderLoader
    }
    SortedOrders(const UnsortedOrders<ASCENDING>& v)
    {
        for (auto& o : v) {
            // HistoryId id;
            // AccountId aid;
            // PinHeight pinHeight;
            // NonceId nonceId;
            // defi::Order_uint64 order;
            // Funds_uint64 filled;
            push_back(OrderData(
                o.orderId,
                o.txid,
                o.order.amount.funds,
                Funds_uint64::zero(),
                o.order.limit));
        }
        std::sort(begin(), end(), &SortedOrders<ASCENDING>::in_order);
    }
};
using SortedOrdersPriceAsc = SortedOrders<true>;
using SortedOrdersPriceDesc = SortedOrders<false>;

template <bool ASCENDING>
class OrderMergeLoader {
    using loader_t = OrderLoader<ASCENDING>;
    using sorted_t = SortedOrders<ASCENDING>;

    void prefetch_old()
    {
        if (loaderDrained || o_old.has_value())
            return;
        o_old = loader();
        if (!o_old)
            loaderDrained = true;
    }

    void prefetch_new()
    {
        if (nextOrder != newOrders.end() && !o_new.has_value())
            o_new = *(nextOrder++);
    }

public:
    OrderMergeLoader(loader_t loader, const UnsortedOrders<ASCENDING>& orders)
        : loader(std::move(loader))
        , newOrders(orders)
        , nextOrder(newOrders.begin())
    {
    }
    std::optional<OrderData> operator()()
    {
        std::optional<OrderData> res;
        prefetch_new();
        prefetch_old();
        if (o_old && (!o_new || sorted_t::in_order(*o_old, *o_new))) {
            res = o_old;
            o_old.reset();
        } else {
            res = o_new;
            o_new.reset();
        }
        return res;
    }

private:
    loader_t loader; // order loader that loads from database
    std::optional<OrderData> o_old;
    std::optional<OrderData> o_new;
    bool loaderDrained { false };
    sorted_t newOrders; // orders not yet in block
    sorted_t::const_iterator nextOrder;
};

template <bool ASCENDING>
struct OrderAggregator {
    using loader_t = OrderLoader<ASCENDING>;
    OrderAggregator(loader_t loader, const UnsortedOrders<ASCENDING>& orders)
        : OrderAggregator(OrderMergeLoader<ASCENDING> { std::move(loader), orders })
    {
    }
    OrderAggregator(OrderMergeLoader<ASCENDING> l)
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
    auto& loaded_orders() const { return loaded; }

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
    OrderMergeLoader<ASCENDING> l;
};
using BuyOrderAggregator = OrderAggregator<false>;
using SellOrderAggregator = OrderAggregator<true>;

struct AggregatorMatch {
    SellOrderAggregator sellAscAggregator;
    BuyOrderAggregator buyDescAggregator;
    defi::MatchResult_uint64 m;
    AggregatorMatch(const ChainDB& db, const UnsortedOrderbook& unsortedOrderbook, TokenId tid, const defi::PoolLiquidity_uint64& p)
        : sellAscAggregator { db.base_order_loader_ascending(tid), unsortedOrderbook.sells }
        , buyDescAggregator { db.quote_order_loader_descending(tid), unsortedOrderbook.buys }
        , m { defi::match_lazy(sellAscAggregator, buyDescAggregator, p) }
    {
    }
};

struct MatchStateDelta {
    TokenId tokenId;
    defi::PoolLiquidity_uint64 pool;
    std::vector<chain_db::OrderFillstate> orderFillstates;
    std::vector<chain_db::OrderDelete> orderDeletes;
    MatchStateDelta(TokenId tokenId, defi::PoolLiquidity_uint64 pool)
        : tokenId(tokenId)
        , pool(std::move(pool))
    {
    }
};

struct MatchActions : public MatchStateDelta {
private:
    MatchActions(const AggregatorMatch& m, TokenId tokenId, const defi::PoolLiquidity_uint64& p);

public:
    MatchActions(const ChainDB& db, const UnsortedOrderbook& unsortedOrderbook, TokenId tid, const defi::PoolLiquidity_uint64& p)
        : MatchActions(AggregatorMatch { db, unsortedOrderbook, tid, p }, tid, p)
    {
    }

    // state changes

    std::vector<BuySwapInternal> buySwaps;
    std::vector<SellSwapInternal> sellSwaps;
};

MatchActions::MatchActions(const AggregatorMatch& am, TokenId tokenId, const defi::PoolLiquidity_uint64& p)
    : MatchStateDelta { tokenId, p }
{
    auto& m { am.m };
    Funds_uint64 fromPool { 0 };
    defi::BaseQuote_uint64 returned { m.filled };
    if (m.toPool) {
        auto pa { m.toPool->amount };
        if (m.toPool->isQuote) {
            returned.quote.subtract_assert(pa);
            fromPool = pool.buy(pa, 50);
            returned.base.add_assert(fromPool);
        } else {
            returned.base.subtract_assert(pa);
            fromPool = pool.sell(pa, 50);
            returned.quote.add_assert(fromPool);
        }
    }

    { // seller match
        Funds_uint64 quoteDistributed { 0 };
        auto remaining { m.filled.base };
        for (auto& o : am.sellAscAggregator.loaded_orders()) {
            if (remaining == 0)
                break;
            auto orderFilled { o.filled };
            auto b { std::min(remaining, o.remaining()) };
            orderFilled.add_assert(b);
            remaining.subtract_assert(b);

            // compute return of that order
            auto q { Prod128(b, returned.quote).divide_floor(m.filled.base.value()) };
            assert(q.has_value());
            quoteDistributed.add_assert(*q);

            if (orderFilled >= o.order.amount) {
                assert(orderFilled == o.order.amount);
                orderDeletes.push_back({ .id { o.id }, .buy = false });
            } else {
                orderFillstates.push_back({ .id { o.id }, .buy = false, .filled { orderFilled } });
            }
            sellSwaps.push_back(SellSwapInternal { { .oId { o.id }, .txid { o.txid }, .base { b }, .quote { Wart::from_funds_throw(*q) } } });
            // order swapped b -> q
        }
        assert(remaining == 0);
        returned.quote.subtract_assert(quoteDistributed);
    }
    { // buyer match
        Funds_uint64 baseDistributed { 0 };
        auto remaining { m.filled.quote };
        for (auto& o : am.buyDescAggregator.loaded_orders()) {
            if (remaining == 0)
                break;
            auto orderFilled { o.filled };
            auto q { std::min(remaining, o.remaining()) };
            orderFilled.add_assert(q);
            remaining.subtract_assert(q);

            // compute return of that order
            auto b { Prod128(q, returned.base).divide_floor(m.filled.quote.value()) };
            assert(b.has_value());
            baseDistributed.add_assert(*b);

            // order swapped q -> b
            if (orderFilled >= o.order.amount) {
                assert(orderFilled == o.order.amount);
                orderDeletes.push_back({ .id { o.id }, .buy = true });
            } else {
                orderFillstates.push_back({ .id { o.id }, .buy = true, .filled { orderFilled } });
            }
            buySwaps.push_back(BuySwapInternal { { .oId { o.id }, .txid { o.txid }, .base { *b }, .quote { Wart::from_funds_throw(q) } } });
        }
        assert(remaining == 0);
        returned.base.subtract_assert(baseDistributed);
    }
}
struct NewOrdersInternal : UnsortedOrderbook {
    bool empty() const { return buys.empty() && sells.empty(); }
    void push_back(VerifiedOrderWithId o)
    {
        if (o.order.buy)
            buys.push_back(std::move(o));
        else
            sells.push_back(std::move(o));
    }
};

struct TokenSectionInternal {
    TokenId id;
    std::vector<TokenTransferInternal> transfers;
    std::vector<OrderInternal> orders;
    std::vector<CancelationInternal> cancelations;
    std::vector<LiquidityAddInternal> liquidityAdds;
    std::vector<LiquidityRemoveInternal> liquidityRemoves;
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
            , end(beginNew + bv.addresses().size())
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
    class StateIdIncrementer {
        StateId next;

    public:
        StateIdIncrementer(StateId next)
            : next(next)
        {
        }
        auto get_next() { return next++; }
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
    BalanceChecker(AccountId nextAccountId, StateId nextStateId, const BodyView& bv, NonzeroHeight height)
        : height(height)
        , pinFloor(height.pin_floor())
        , bv(bv)
        , accounts(nextAccountId, bv)
        , stateId(nextStateId)
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

    struct ProcessedSigner : public SignerData {
        AccountData& account;
    };
    template <typename ItemView>
    [[nodiscard]] ProcessedSigner process_signer(const ItemView& v)
    {
        auto compactFee { v.compact_fee_throw() };
        AccountData& from { accounts[v.origin_account_id()] };
        charge_fee(from, compactFee);
        return {
            SignerData { from.id, from.address, v.pin_nonce(), compactFee, v.signature() },
            from
        };
    }
    TransferInternalWithoutAmount __register_transfer(view::TokenTransfer tv) // OK
    {
        auto s { process_signer(tv) };
        auto tokenId { tv.id };
        auto amount { tv.amount_throw() };
        auto& to { accounts[tv.toAccountId()] };
        if (height.value() > 719118 && amount.is_zero())
            throw Error(EZEROAMOUNT);
        if (s.origin.id == to.id)
            throw Error(ESELFSEND);

        to.add(tokenId, amount);
        s.account.subtract(tokenId, amount);

        return {
            std::move(s),
            { to.id, to.address },
        };
    }
    void register_wart_transfer(view::WartTransfer tv)
    {
        wartTransfers.push_back({ __register_transfer(tv), tv.amount_throw() });
    }

    CancelationInternal register_cancelation(view::Cancelation c)
    {
        auto signerData(process_signer(c));
        return {
            signerData,
            c.token_id(),
            { signerData.origin.id, c.block_pin_nonce().pin_height_from_floored(pinFloor), c.block_pin_nonce().id }
        };
    }
    LiquidityAddInternal register_liquidity_add(view::LiquidityAdd l)
    {
        return { process_signer(l) };
    }
    LiquidityRemoveInternal register_liquidity_remove(view::LiquidityRemove l)
    {
        return { process_signer(l) };
    }
    OrderInternal register_new_order(view::Order o)
    {
        auto s { process_signer(o) };
        auto tokenId { o.token_id() };

        auto [buy, amount] { o.buy_amount_throw() };
        if (buy)
            s.account.subtract(TokenId::WART, amount);
        else
            s.account.subtract(tokenId, amount);

        return {
            std::move(s),
            o.limit(),
            TokenFunds { tokenId, amount },
            buy
        };
    }
    void register_token_section(TokenSectionInteractor t)
    {
        TokenSectionInternal ts(t.id());
        for (auto tr : t.transfers())
            ts.transfers.push_back({ __register_transfer({ tr, t.id() }), tr.amount_throw() });
        for (auto o : t.orders())
            ts.orders.push_back(register_new_order(o));
        for (auto c : t.cancelations())
            ts.cancelations.push_back(register_cancelation(c));
        for (auto a : t.liquidityAdd())
            ts.liquidityAdds.push_back(register_liquidity_add(a));
        for (auto r : t.liquidityRemove())
            ts.liquidityRemoves.push_back(register_liquidity_remove(r));
        tokenSections.push_back(std::move(ts));
    }

    void register_token_creation(Indexed<view::TokenCreation> tc, Height)
    {
        auto s { process_signer(tc) };
        tokenCreations.push_back(TokenCreationInternal {
            s,
            tc.index,
            tc.token_name(),
            FundsDecimal { tc.total_supply(), tc.precision() },
        });
    }

    Wart getTotalFee() const
    {
        return totalfee;
    }; // OK
    auto& old_accounts()
    {
        return accounts.old_accounts();
    }
    auto& get_new_accounts() const
    {
        return accounts.new_accounts();
    }
    auto& get_wart_transfers() const
    {
        return wartTransfers;
    }
    AddressView get_new_address(size_t i)
    {
        return bv.get_address(i);
    } // OK
    auto& token_creations() const
    {
        return tokenCreations;
    }
    const auto& get_token_sections() const
    {
        return tokenSections;
    };
    const auto& get_reward() const
    {
        return reward;
    };

private:
    const NonzeroHeight height;
    const PinFloor pinFloor;
    const BodyView& bv;
    Accounts accounts;
    StateIdIncrementer stateId;
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
    InsertHistoryEntry(const VerifiedOrder& t, HistoryId historyId)
        : he(t)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(const VerifiedCancelation& t, HistoryId historyId)
        : he(t)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(const VerifiedWartTransfer& t, HistoryId historyId)
        : he(t)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(const VerifiedTokenCreation& t, TokenId tokenId, HistoryId historyId)
        : he(t, tokenId)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(ProcessedBuySwap h, HistoryId historyId)
        : he(std::move(h))
        , historyId(historyId)
        , parent(HistoryId(h.oId.value()))
    {
    }
    InsertHistoryEntry(ProcessedSellSwap h, HistoryId historyId)
        : he(std::move(h))
        , historyId(historyId)
        , parent(HistoryId(h.oId.value()))
    {
    }
    history::Entry he;
    HistoryId historyId;
    std::optional<HistoryId> parent;
};

class HistoryIdGenerator {
public:
    HistoryIdGenerator(HistoryId next)
        : next(next)
    {
    }
    HistoryId operator()()
    {
        return next++;
    }

private:
    HistoryId next;
};
struct HistoryEntries {
    void write(ChainDB& db)
    {
        // insert history for payouts and payments
        for (auto& p : insertHistory) {
            auto inserted { db.insertHistory(p.he.hash, p.he.data) };
            if (p.parent)
                db.insert_history_link(*p.parent, p.historyId);
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

struct HistoryEntriesGenerator {
private:
    HistoryEntries& entries;
    HistoryIdGenerator next_id;

public:
    HistoryEntriesGenerator(HistoryEntries& entries, HistoryId nextHistoryId)
        : entries(entries)
        , next_id(nextHistoryId)
    {
    }
    struct ForAccounts {
        HistoryId hid;
        HistoryEntriesGenerator& g;
        template <typename... T>
        auto& insert_history(T&&... t)
        {
            return g.entries.insertHistory.emplace_back(std::forward<T>(t)..., hid);
        }
    };

    ForAccounts for_account(AccountId aid)
    {
        ForAccounts h { next_id(), *this };
        entries.insertAccountHistory.push_back({ aid, h.hid });
        return h;
    };
    ForAccounts for_accounts(AccountId aid1, AccountId aid2)
    {
        ForAccounts h { next_id(), *this };
        entries.insertAccountHistory.push_back({ aid1, h.hid });
        if (aid1 != aid2)
            entries.insertAccountHistory.push_back({ aid2, h.hid });
        return h;
    };

    [[nodiscard]] const auto& push_reward(const RewardInternal& r)
    {
        return for_account(r.toAccountId).insert_history(r);
    }
    [[nodiscard]] auto& push_wart_transfer(const VerifiedWartTransfer& r)
    {
        return for_accounts(r.ti.origin.id, r.ti.to.id).insert_history(r);
    }
    [[nodiscard]] auto& push_order(const VerifiedOrder& r)
    {
        return for_account(r.order.origin.id).insert_history(r);
    }
    [[nodiscard]] auto& push_cancelation(const VerifiedCancelation& r)
    {
        return for_account(r.cancelation.origin.id).insert_history(r);
    }
    [[nodiscard]] auto& push_token_transfer(const VerifiedTokenTransfer& r, TokenId tokenId)
    {
        return for_accounts(r.ti.origin.id, r.ti.to.id).insert_history(r, tokenId);
    }
    [[nodiscard]] auto& push_swap(const BuySwapInternal& t, Height h)
    {
        return for_account(t.txid.accountId).insert_history(ProcessedBuySwap(t, h));
    }
    [[nodiscard]] auto& push_swap(const SellSwapInternal& t, Height h)
    {
        return for_account(t.txid.accountId).insert_history(ProcessedSellSwap(t, h));
    }
};

} // namespace

namespace chainserver {
class Preparation {

public:
    HistoryEntries historyEntries;
    rollback::Data rg;
    std::set<TransactionId> txset;
    std::vector<std::tuple<BalanceId, AccountToken, Funds_uint64>> updateBalances;
    std::vector<std::tuple<AccountToken, Funds_uint64>> insertBalances;
    std::vector<std::tuple<AddressView, AccountId>> insertAccounts;
    std::vector<chain_db::OrderDelete> deleteOrders;
    std::vector<chain_db::TokenData> insertTokenCreations;
    std::vector<chain_db::OrderData> insertOrders;
    std::vector<TransactionId> insertCancelOrder;
    std::vector<MatchStateDelta> matchDeltas;
    api::Block::Actions api;

private:
    friend class PreparationGenerator;
    Preparation(const ChainDB& db)
        : rg(db)
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
    const view::Reward reward;
    BalanceChecker balanceChecker;
    HistoryEntriesGenerator history;
    TransactionVerifier txVerifier;

private:
    // Check uniqueness of new addresses
    void verify_new_address_policy()
    {
        std::set<AddressView> newAddresses;
        for (auto address : bv.addresses()) {
            if (newAddresses.emplace(address).second == false)
                throw Error(EADDRPOLICY);
            if (db.lookup_account_id(address))
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
        for (auto t : bv.tokens())
            balanceChecker.register_token_section(t);
    }

    void register_token_creations()
    {
        for (auto tc : bv.token_creations())
            balanceChecker.register_token_creation(tc, height);

        const auto beginNewTokenId = db.next_token_id(); // they start from this index
        for (auto& tc : balanceChecker.token_creations()) {
            auto tokenId { beginNewTokenId + tc.index };
            const auto verified { tc.verify(txVerifier) };
            insertTokenCreations.push_back(chain_db::TokenData {
                .id { tokenId },
                .height { height },
                .ownerAccountId { tc.origin.id },
                .supply { tc.supply },
                .groupId { tokenId },
                .parentId { TokenId { 0 } },
                .name { tc.name },
                .hash { verified.hash },
                .data {} });
            api.tokenCreations.push_back({
                .txid { verified.txid },
                .txhash { verified.hash },
                .tokenName { tc.name },
                .supply { tc.supply },
                .tokenId { tokenId },
                .fee { tc.compactFee.uncompact() },
            });
        }
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
        throw Error(EACCIDNOTFOUND); // invalid account id (not found in database)
    }
    auto db_token(TokenId id)
    {
        if (auto address { db.lookup_token(id) })
            return *address;
        throw Error(ETOKIDNOTFOUND); // invalid token id (not found in database)
    }

    void process_accounts()
    {
        // process old accounts
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

        // process new accounts
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

    void process_actions()
    {
        process_reward();
        process_wart_transfers();
        process_token_sections();
    }

    void process_reward()
    {
        const auto& balanceChecker { this->balanceChecker }; // shadow balanceChecker

        auto& r { balanceChecker.get_reward() };
        if (r.amount > Wart::sum_throw(height.reward(), balanceChecker.getTotalFee()))
            throw Error(EBALANCE);
        assert(!r.toAddress.is_null());
        auto& ref { history.push_reward(r) };
        api.reward = api::Block::Reward {
            .txhash { ref.he.hash },
            .toAddress { r.toAddress },
            .amount { r.amount },
        };
    }

    auto verify_txid(TransactionId tid) -> bool
    {
        // check for duplicate txid (also within current block)
        return !baseTxIds.contains(tid) && !newTxIds.contains(tid) && txset.emplace(tid).second;
    }

    struct TokenData : TokenIdHashNamePrecision {
        TokenData(TokenIdHashNamePrecision t)
            : TokenIdHashNamePrecision(std::move(t))
        {
        }
        const auto& get_pool(const ChainDB& db) const
        {
            if (!loaded) {
                loaded = true;
                _l = db.select_pool(id);
            }
            return _l;
        }

    private:
        mutable bool loaded { false };
        mutable std::optional<PoolData> _l;
    };
    void process_token_sections()
    {
        auto ts { balanceChecker.get_token_sections() };
        for (auto& ts : balanceChecker.get_token_sections()) {
            auto ihn { db_token(ts.id).id_hash_name_precision() };
            process_token_transfers(ihn, ts.transfers);
            process_new_orders(ihn, ts.orders);
            process_cancelations(ihn, ts.cancelations);
            process_liquidity_adds(ihn, ts.liquidityAdds);
            process_liquidity_removes(ihn, ts.liquidityRemoves);
        }
    }
    template <typename... T>
    [[nodiscard]] auto verify(auto& tx, T&&... t)
    {
        return tx.verify(txVerifier, std::forward<T>(t)...);
    }
    // generate history for transfers and check signatures
    // and check for unique transaction ids
    void process_wart_transfers()
    {
        const auto& balanceChecker { this->balanceChecker }; // shadow balanceChecker
        for (auto& tr : balanceChecker.get_wart_transfers()) {
            auto verified { verify(tr) };

            auto& ref { history.push_wart_transfer(verified) };
            api.wartTransfers.push_back(api::Block::Transfer {
                .fromAddress { tr.origin.address },
                .fee { tr.compactFee.uncompact() },
                .nonceId { tr.pinNonce.id },
                .pinHeight { verified.txid.pinHeight },
                .txhash { ref.he.hash },
                .toAddress { tr.to.address },
                .amount { tr.amount },
            });
        }
    }

    void process_token_transfers(const TokenIdHashNamePrecision& token, const std::vector<TokenTransferInternal>& transfers)
    {
        for (auto& tr : transfers) {
            auto verified { verify(tr, token.hash) };

            auto& ref { history.push_token_transfer(verified, token.id) };
            api.tokenTransfers.push_back(api::Block::TokenTransfer {
                .tokenInfo { token },
                .fromAddress { tr.origin.address },
                .fee { tr.compactFee.uncompact() },
                .nonceId { tr.pinNonce.id },
                .pinHeight { verified.txid.pinHeight },
                .txhash { ref.he.hash },
                .toAddress { tr.to.address },
                .amount { tr.amount, token.precision },
            });
        }
    }
    void process_new_orders(const TokenData& token, const std::vector<OrderInternal>& orders)
    {
        if (orders.size() == 0)
            return;
        auto td { token.get_pool(db) };
        if (!td)
            throw Error(ENOPOOL);
        auto poolLiquidity { td->liquidity() };
        NewOrdersInternal no;
        for (auto& o : orders) {
            auto verified { verify(o, token.hash) };
            auto& ref { history.push_order(verified) };
            api.newOrders.push_back(api::Block::NewOrder { .tokenInfo { token },
                .fee { o.compactFee.uncompact() },
                .amount { o.amount.funds, token.precision },
                .limit { o.limit },
                .buy = o.buy,
                .txid { verified.txid },
                .txhash { verified.hash },
                .address { o.origin.address } });
            no.push_back({ verified, ref.historyId });
            insertOrders.push_back(chain_db::OrderData {
                .id { ref.historyId },
                .buy = o.buy,
                .txid { verified.txid },
                .tid { token.id },
                .total { o.amount.funds },
                .filled { Funds_uint64::zero() },
                .limit { o.limit } });
        }

        MatchActions m(db, no, token.id, poolLiquidity);

        for (auto& s : m.buySwaps) {
            auto& ref { history.push_swap(s, height) };
            api.swaps.push_back(api::Block::Swap {
                .tokenInfo { token },
                .txhash { ref.he.hash },
                .buy = true,
                .fillQuote { s.quote },
                .fillBase { s.base, token.precision },
            });
        }
        for (auto& s : m.sellSwaps) {
            auto& ref { history.push_swap(s, height) };
            api.swaps.push_back({
                .tokenInfo { token },
                .txhash { ref.he.hash },
                .buy = false,
                .fillQuote { s.quote },
                .fillBase { s.base, token.precision },
            });
        }
        matchDeltas.push_back(std::move(m));
    }
    void process_cancelations(const TokenIdHashNamePrecision& token, const std::vector<CancelationInternal>& cancelations)
    {
        for (auto& c : cancelations) {
            auto verified { verify(c, token.hash) };
            auto& ref { history.push_cancelation(verified) };
            api.cancelations.push_back(
                { .origin { c.origin },
                    .fee { c.compactFee.uncompact() },
                    .txhash { ref.he.hash },
                    .order {} });
            auto o { db.select_order(verified.cancelation.cancelTxid) };
            if (o) { // transaction is removed from the database
                deleteOrders.push_back({ o->id, o->buy });
                api.cancelations.back().order = api::Block::Cancelation::OrderData {
                    .tid { o->tid },
                    .total { o->total },
                    .filled { o->filled },
                    .limit { o->limit },
                    .buy = o->buy
                };
            }
        }
    }
    void process_liquidity_adds(const TokenIdHashNamePrecision& ihn, const std::vector<LiquidityAddInternal>& orders)
    {
    }
    void process_liquidity_removes(const TokenIdHashNamePrecision& ihn, const std::vector<LiquidityRemoveInternal>& orders)
    {
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
        , balanceChecker(db.next_account_id(), db.next_state_id(), bv, height)
        , history(historyEntries, db.next_history_id())
        , txVerifier(TransactionVerifier { hc, height,
              std::function<bool(TransactionId)>(
                  [this](TransactionId tid) -> bool {
                      return verify_txid(tid);
                  }) })
    {

        /// Read block sections
        verify_new_address_policy(); // new address section
        register_wart_transfers(); // WART transfer section
        register_token_sections();
        register_token_creations(); // token creation section

        /// Process block sections
        process_accounts();
        process_actions();
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

        for (auto& [address, accId] : prepared.insertAccounts) // new accounts
            db.insert_account(address, accId);

        for (auto& d : prepared.deleteOrders) // delete orders
            db.delete_order(d);

        for (auto& o : prepared.insertOrders) // new orders
            db.insert_order(o);
        for (auto& d : prepared.matchDeltas) {
            for (auto& o : d.orderFillstates) // update fillstates
                db.change_fillstate(o);
            for (auto& o : d.orderDeletes) // delete filled orders
                db.delete_order(o);
            db.set_pool_liquidity(d.tokenId, d.pool);
        }

        // insert token creations
        for (auto& tc : prepared.insertTokenCreations)
            db.insert_new_token(tc);

        // insert new balances
        for (auto& [at, bal] : prepared.insertBalances) {
            db.insert_token_balance(at, bal);
            balanceUpdates.insert_or_assign(at, bal);
        }

        // write undo data
        db.set_block_undo(blockId, prepared.rg.serialize());

        // write consensus data
        db.insert_consensus(b.height, blockId, db.next_history_id(), prepared.rg.next_state_id());

        prepared.historyEntries.write(db);
        return api::Block(b.header, b.height, 0, std::move(prepared.api));
    } catch (Error e) {
        throw std::runtime_error(std::string("Unexpected exception: ") + __PRETTY_FUNCTION__ + ":" + e.strerror());
    }
}
}
