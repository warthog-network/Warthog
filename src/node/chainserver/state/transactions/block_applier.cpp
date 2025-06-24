#include "block_applier.hpp"
#include "api/types/all.hpp"
#include "block/body/rollback.hpp"
#include "defi/serialize.hpp"
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
                o.order.amount,
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
    AggregatorMatch(const ChainDB& db, const UnsortedOrderbook& unsortedOrderbook, AssetId aid, const defi::PoolLiquidity_uint64& p)
        : sellAscAggregator { db.base_order_loader_ascending(aid), unsortedOrderbook.sells }
        , buyDescAggregator { db.quote_order_loader_descending(aid), unsortedOrderbook.buys }
        , m { defi::match_lazy(sellAscAggregator, buyDescAggregator, p) }
    {
    }
};

struct MatchStateDelta {
    AssetId assetId;
    defi::PoolLiquidity_uint64 pool; // pool liquidity after match
    std::vector<chain_db::OrderDelete> orderDeletes;
    std::optional<chain_db::OrderFillstate> orderBuyPartial;
    std::optional<chain_db::OrderFillstate> orderSellPartial;
    MatchStateDelta(AssetId assetId, defi::PoolLiquidity_uint64 pool)
        : assetId(assetId)
        , pool(std::move(pool))
    {
    }
};

struct MatchActions : public MatchStateDelta {
private:
    MatchActions(const AggregatorMatch& m, AssetId assetId, const defi::PoolLiquidity_uint64& p);

public:
    MatchActions(const ChainDB& db, const UnsortedOrderbook& unsortedOrderbook, AssetId aid, const defi::PoolLiquidity_uint64& p)
        : MatchActions(AggregatorMatch { db, unsortedOrderbook, aid, p }, aid, p)
    {
    }

    // state changes

    std::vector<BuySwapInternal> buySwaps;
    std::vector<SellSwapInternal> sellSwaps;
};

MatchActions::MatchActions(const AggregatorMatch& am, AssetId assetId, const defi::PoolLiquidity_uint64& p)
    : MatchStateDelta { assetId, p }
{
    auto& m { am.m };
    Funds_uint64 fromPool { 0 };
    defi::BaseQuote_uint64 returned { m.filled };
    if (m.toPool) {
        auto pa { m.toPool->amount() };
        if (m.toPool->is_quote()) {
            returned.quote.subtract_assert(pa);
            fromPool = pool.buy(pa, 50);
            returned.base.add_assert(fromPool);
        } else {
            returned.base.subtract_assert(pa);
            fromPool = pool.sell(pa, 50);
            returned.quote.add_assert(fromPool);
        }
    }

    if (m.filled.base != 0) { // seller match
        Nonzero_uint64 filledBase { m.filled.base.value() };
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
            auto q { Prod128(b, returned.quote).divide_floor(filledBase) };
            assert(q.has_value());
            quoteDistributed.add_assert(*q);

            if (orderFilled >= o.order.amount) {
                assert(orderFilled == o.order.amount);
                orderDeletes.push_back({ .id { o.id }, .buy = false });
            } else {
                assert(remaining == 0);
                orderSellPartial.emplace(chain_db::OrderFillstate { .id { o.id }, .filled { orderFilled } });
            }
            sellSwaps.push_back(SellSwapInternal { { .oId { o.id }, .txid { o.txid }, .base { b }, .quote { Wart::from_funds_throw(*q) } } });
            // order swapped b -> q
        }
        assert(remaining == 0);
        returned.quote.subtract_assert(quoteDistributed);
    }
    if (m.filled.quote != 0) { // buyer match
        Nonzero_uint64 filledQuote { m.filled.quote.value() };
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
            auto b { Prod128(q, returned.base).divide_floor(filledQuote) };
            assert(b.has_value());
            baseDistributed.add_assert(*b);

            // order swapped q -> b
            if (orderFilled >= o.order.amount) {
                assert(orderFilled == o.order.amount);
                orderDeletes.push_back({ .id { o.id }, .buy = true });
            } else {
                assert(remaining == 0);
                orderBuyPartial.emplace(chain_db::OrderFillstate { .id { o.id }, .filled { orderFilled } });
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
private:
    AssetId id;

public:
    auto asset_id() const { return id; }
    auto share_id() const { return id.share_id(); }
    std::vector<TokenTransferInternal> assetTransfers;
    std::vector<TokenTransferInternal> sharesTransfers;
    std::vector<OrderInternal> orders;
    std::vector<LiquidityDepositsInternal> liquidityAdds;
    std::vector<LiquidityWithdrawalInternal> liquidityRemoves;
    TokenSectionInternal(AssetId id)
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
        Accounts(AccountId nextAccountId, const block::Body& b)
            : beginNew(nextAccountId)
            , end(beginNew + b.newAddresses.size())
        {
            size_t n { end.value() - beginNew.value() };
            newAccounts.reserve(n);
            for (size_t i = 0; i < n; ++i)
                newAccounts.push_back({ b.newAddresses[i], (beginNew + i).validate_throw(end) });
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
    static RewardInternal register_reward(const block::Body& b, Accounts& accounts, NonzeroHeight h)
    {
        auto r { b.reward };
        auto& a { accounts[r.to_id()] };
        auto am { r.wart() };
        a.add(TokenId::WART, am);
        return {
            .toAccountId { a.id },
            .amount { am },
            .height { h },
            .toAddress { a.address }
        };
    }
    BalanceChecker(AccountId nextAccountId, StateId nextStateId, const block::Body& b, NonzeroHeight height)
        : height(height)
        , pinFloor(height.pin_floor())
        , b(b)
        , accounts(nextAccountId, b)
        , stateId(nextStateId)
        , reward(register_reward(b, accounts, height))
    {
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
        auto compactFee { v.compact_fee() };
        AccountData& from { accounts[v.origin_account_id()] };
        charge_fee(from, compactFee);
        return {
            SignerData { from.id, from.address, v.pin_nonce(), compactFee, v.signature() },
            from
        };
    }
    TransferInternalWithoutAmount __register_transfer(TokenId tokenId, AccountId toId, Funds_uint64 amount, ProcessedSigner s) // OK
    {
        auto& to { accounts[toId] };
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
    void register_wart_transfer(WartTransfer tv)
    {
        wartTransfers.push_back({ __register_transfer(TokenId::WART, tv.to_id(), tv.wart(), process_signer(tv)), tv.wart() });
    }

    void register_cancelation(Cancelation c)
    {
        auto signerData(process_signer(c));
        cancelations.push_back({ signerData,
            { signerData.origin.id, c.block_pin_nonce().pin_height_from_floored(pinFloor), c.block_pin_nonce().id } });
    }
    LiquidityDepositsInternal register_liquidity_deposit(const LiquidityDeposit& l, AssetId aid)
    {
        auto& a { accounts[l.origin_account_id()] };
        a.subtract(aid.token_id(), l.base_amount());
        a.subtract(TokenId::WART, l.quote_wart());

        return { process_signer(l), defi::BaseQuote { l.base_amount(), l.quote_wart() } };
    }
    void add_balance(AccountId aid, TokenId tid, Funds_uint64 amount)
    {
        accounts[aid].add(tid, amount);
    }

    LiquidityWithdrawalInternal register_liquidity_withdraw(LiquidityWithdraw l, ShareId shareId)
    {
        auto& a { accounts[l.origin_account_id()] };
        a.subtract(shareId.token_id(), l.amount());
        return { process_signer(l), l.amount() };
    }
    OrderInternal register_new_order(Order o, AssetId aid)
    {
        auto s { process_signer(o) };

        auto buy { o.buy() };
        auto amount { o.amount() };
        if (buy)
            s.account.subtract(TokenId::WART, amount);
        else
            s.account.subtract(aid.token_id(), amount);

        return {
            std::move(s),
            o.limit(),
            amount,
            aid,
            buy
        };
    }
    void register_token_section(const TokenSection& t)
    {
        auto aid { t.asset_id() };
        auto sid { t.share_id() };
        TokenSectionInternal ts(t.asset_id());
        for (auto& tr : t.assetTransfers)
            ts.assetTransfers.push_back({ __register_transfer(aid.token_id(), tr.to_id(), tr.amount(), process_signer(tr)), tr.amount() });
        for (auto& tr : t.shareTransfers)
            ts.sharesTransfers.push_back({ __register_transfer(sid.token_id(), tr.to_id(), tr.amount(), process_signer(tr)), tr.amount() });
        for (auto& o : t.orders)
            ts.orders.push_back(register_new_order(o, aid));
        for (auto& a : t.liquidityAdd)
            ts.liquidityAdds.push_back(register_liquidity_deposit(a, t.asset_id()));
        for (auto& r : t.liquidityRemove)
            ts.liquidityRemoves.push_back(register_liquidity_withdraw(r, t.share_id()));
        tokenSections.push_back(std::move(ts));
    }

    void register_token_creation(const AssetCreation& tc, size_t index, Height)
    {
        auto s { process_signer(tc) };
        tokenCreations.push_back(TokenCreationInternal {
            s,
            index,
            tc.asset_name(),
            tc.supply(),
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
    auto& get_cancelations() const
    {
        return cancelations;
    }
    AddressView get_new_address(size_t i)
    {
        return b.newAddresses[i];
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
    const block::Body& b;
    Accounts accounts;
    StateIdIncrementer stateId;
    RewardInternal reward;

    std::vector<WartTransferInternal> wartTransfers;
    std::vector<CancelationInternal> cancelations;
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
    InsertHistoryEntry(const VerifiedAssetCreation& t, AssetId assetId, HistoryId historyId)
        : he(t, assetId)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(const VerifiedLiquidityDeposit& t, Funds_uint64 receivedShares, AssetId assetId, HistoryId historyId)
        : he(t, receivedShares, assetId)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(const VerifiedLiquidityWithdrawal& t, Funds_uint64 receivedBase, Wart receivedQuote, AssetId assetId, HistoryId historyId)
        : he(t, receivedBase, receivedQuote, assetId)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(history::MatchData md, Hash h, HistoryId historyId)
        : he(std::move(h), std::move(md))
        , historyId(historyId)
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
            auto inserted { db.insertHistory(p.he.hash, p.he.data.serialize()) };
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
    ForAccounts for_accounts(const std::vector<AccountId>& accounts)
    {
        ForAccounts h { next_id(), *this };
        for (auto& aid : accounts)
            entries.insertAccountHistory.push_back({ aid, h.hid });
        return h;
    };
    // ForAccounts for_accounts(const auto& account_id_generator)
    // {
    //     ForAccounts h { next_id(), *this };
    //     while (true) {
    //         if (std::optional<AccountId> aid { account_id_generator() })
    //             entries.insertAccountHistory.push_back({ *aid, h.hid });
    //         else
    //             break;
    //     }
    //     return h;
    // };

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
    [[nodiscard]] auto& push_asset_creation(const VerifiedAssetCreation& t, AssetId assetId)
    {
        return for_account(t.tci.origin.id).insert_history(t, assetId);
    }
    [[nodiscard]] auto& push_match(const std::vector<AccountId>& accounts, const history::MatchData& t, const BlockHash& blockHash, AssetId assetId)
    {
        return for_accounts(accounts).insert_history(t, hash_args_SHA256(blockHash, assetId));
    }
    [[nodiscard]] auto& push_liquidity_deposit(const VerifiedLiquidityDeposit& v, Funds_uint64 sharesReceived, AssetId aid)
    {
        return for_account(v.liquidityAdd.origin.id).insert_history(v, sharesReceived, aid);
    }
    [[nodiscard]] auto& push_liquidity_withdrawal(const VerifiedLiquidityWithdrawal& v, Funds_uint64 baseReceived, Wart quoteReceived, AssetId assetId)
    {
        return for_account(v.liquidityAdd.origin.id).insert_history(v, baseReceived, quoteReceived, assetId);
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
    std::vector<chain_db::AssetData> insertAssetCreations;
    std::vector<chain_db::OrderData> insertOrders;
    std::vector<TransactionId> insertCancelOrder;
    std::vector<MatchStateDelta> matchDeltas;
    api::block::Actions api;

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
    const BlockHash& blockhash;
    const block::Body& body;
    const NonzeroHeight height;

    // variables needed for block verification
    const Reward reward;
    BalanceChecker balanceChecker;
    HistoryEntriesGenerator history;
    TransactionVerifier txVerifier;

private:
    // Check uniqueness of new addresses
    void verify_new_address_policy()
    {
        std::set<AddressView> newAddresses;
        for (auto address : body.newAddresses) {
            if (newAddresses.emplace(address).second == false)
                throw Error(EADDRPOLICY);
            if (db.lookup_account(address))
                throw Error(EADDRPOLICY);
        }
    }

    void register_wart_transfers()
    {
        // Read transfer section for WART coins
        for (auto t : body.wartTransfers)
            balanceChecker.register_wart_transfer(t);
    }
    void register_cancelations()
    {
        for (auto& c : body.cancelations)
            balanceChecker.register_cancelation(c);
    }
    void register_token_sections()
    {
        for (auto t : body.tokens)
            balanceChecker.register_token_section(t);
    }

    void register_token_creations()
    {
        for (size_t i { 0 }; i < body.tokenCreations.size(); ++i)
            balanceChecker.register_token_creation(body.tokenCreations[i], i, height);

        const auto beginNewTokenId = db.next_asset_id(); // they start from this index
        for (auto& tc : balanceChecker.token_creations()) {
            auto assetId { beginNewTokenId + tc.index };
            const auto verified { tc.verify(txVerifier) };
            insertAssetCreations.push_back(chain_db::AssetData {
                .id { assetId },
                .height { height },
                .ownerAccountId { tc.origin.id },
                .supply { tc.supply },
                .groupId { assetId.token_id() },
                .parentId { TokenId { 0 } },
                .name { tc.name },
                .hash { verified.hash },
                .data {} });
            auto& ref { history.push_asset_creation(verified, assetId) };
            api.assetCreations.push_back({
                .txhash { ref.he.hash },
                .assetName { tc.name },
                .supply { tc.supply },
                .assetId { assetId },
                .fee { tc.compactFee.uncompact() },
            });
        }
    }

    auto validate_new_balance(const AccountToken& at, const auto& tokenFlow)
    {
        if (tokenFlow.out() > Funds_uint64::zero()) // We do not allow resend of newly inserted balance
            throw Error(EBALANCE); // insufficient balance
        Funds_uint64 balance { tokenFlow.in() };
        insertBalances.push_back({ at, balance });
    }
    auto validate_existing_balance(const AccountToken& at, const auto& tokenFlow, const std::pair<BalanceId, Funds_uint64>& b)
    {
        const auto& [balanceId, balance] { b };
        rg.register_balance(balanceId, balance);
        // check that balances are correct
        auto totalIn { Funds_uint64::sum_throw(tokenFlow.in(), balance) };
        Funds_uint64 newbalance { Funds_uint64::diff_throw(totalIn, tokenFlow.out()) };
        updateBalances.push_back({ balanceId, at, newbalance });
    }
    auto db_addr(AccountId id)
    {
        if (auto address { db.lookup_address(id) })
            return *address;
        throw Error(EACCIDNOTFOUND); // invalid account id (not found in database)
    }
    auto db_asset(AssetId id)
    {
        if (auto address { db.lookup_asset(id) })
            return *address;
        throw Error(ETOKIDNOTFOUND); // invalid token id (not found in database)
    }

    void process_balances()
    {
        // process old accounts
        for (auto& [accountId, accountData] : balanceChecker.old_accounts()) {
            accountData.address = db_addr(accountId);
            for (auto& [tokenId, tokenFlow] : accountData.token_flow()) {
                AccountToken at { accountId, tokenId };
                if (auto p { db.get_balance(accountId, tokenId) })
                    validate_existing_balance(at, tokenFlow, *p);
                else
                    validate_new_balance(at, tokenFlow);
            }
        }

        // process new accounts
        for (auto& a : balanceChecker.get_new_accounts()) {
            bool referred { false };
            for (auto& [tokenId, tokenFlow] : a.token_flow()) {
                if (!tokenFlow.in().is_zero())
                    referred = true;
                validate_new_balance({ a.id, tokenId }, tokenFlow);
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
        process_cancelations();
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
        api.reward = api::block::Reward {
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

    struct AssetHandle {
        struct LoadedPool {
            bool create;
            PoolData pool;
        };
        AssetHandle(AssetIdHashNamePrecision t)
            : _info(std::move(t))
        {
        }
        auto& info() const { return _info; }
        auto& hash() const { return info().hash; }
        auto& precision() const { return info().precision; }
        auto id() const { return _info.id; }
        auto share_id() const { return id().share_id(); }
        auto& name() const { return _info.name; }
        [[nodiscard]] auto& get_pool(const ChainDB& db) const
        {
            if (!o) {
                if (auto p { db.select_pool(info().id) })
                    o.emplace(LoadedPool { false, *p });
                else
                    o.emplace(LoadedPool { true, PoolData::zero(id()) });
            }
            return o->pool;
        }

    private:
        AssetIdHashNamePrecision _info;
        mutable std::optional<LoadedPool> o;
    };
    void process_token_sections()
    {
        auto ts { balanceChecker.get_token_sections() };
        for (auto& ts : balanceChecker.get_token_sections()) {
            auto ihn { db_asset(ts.asset_id()).id_hash_name_precision() };
            AssetHandle th(ihn);
            process_token_transfers(th, ts.sharesTransfers);
            match_new_orders(th, ts.orders);
            process_liquidity_deposits(th, ts.liquidityAdds);
            process_liquidity_withdrawals(th, ts.liquidityRemoves);
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
            api.wartTransfers.push_back(api::block::WartTransfer {
                .txhash { ref.he.hash },
                .fromAddress { tr.origin.address },
                .fee { tr.compactFee.uncompact() },
                .nonceId { tr.pinNonce.id },
                .pinHeight { verified.txid.pinHeight },
                .toAddress { tr.to.address },
                .amount { tr.amount },
            });
        }
    }

    void process_cancelations()
    {
        const auto& balanceChecker { this->balanceChecker }; // const lock balanceChecker
        for (auto& c : balanceChecker.get_cancelations()) {
            auto verified { verify(c) };
            auto& ref { history.push_cancelation(verified) };
            api.cancelations.push_back(
                // Hash txhash;
                // Wart fee;
                // Address address;
                {
                    .txhash { ref.he.hash },
                    .fee { c.compactFee.uncompact() },
                    .address { c.origin.address },
                });
            auto o { db.select_order(verified.cancelation.cancelTxid) };
            if (o) { // transaction is removed from the database
                deleteOrders.push_back({ o->id, o->buy });
            }
        }
    }

    void process_token_transfers(AssetHandle& token, const std::vector<TokenTransferInternal>& transfers)
    {
        for (auto& tr : transfers) {
            auto verified { verify(tr, token.info().hash) };

            auto& ref { history.push_token_transfer(verified, token.id().token_id()) };
            api.tokenTransfers.push_back(api::block::TokenTransfer {
                .txhash { ref.he.hash },
                .assetInfo { token.info() },
                .fromAddress { tr.origin.address },
                .fee { tr.compactFee.uncompact() },
                .nonceId { tr.pinNonce.id },
                .pinHeight { verified.txid.pinHeight },
                .toAddress { tr.to.address },
                .amount { tr.amount },
            });
        }
    }
    void match_new_orders(AssetHandle& asset, const std::vector<OrderInternal>& orders)
    {
        if (orders.size() == 0)
            return;
        auto& p { asset.get_pool(db) };
        auto poolLiquidity { p.liquidity() };
        NewOrdersInternal no;
        for (auto& o : orders) {
            auto verified { verify(o, asset.hash()) };
            auto& ref { history.push_order(verified) };
            api.newOrders.push_back(api::block::NewOrder {
                .txhash { verified.hash },
                .assetInfo { asset.info() },
                .fee { o.fee() },
                .amount { o.amount },
                .limit { o.limit },
                .buy = o.buy,
                .address { o.origin.address } });
            no.push_back({ verified, ref.historyId });
            insertOrders.push_back(chain_db::OrderData {
                .id { ref.historyId },
                .buy = o.buy,
                .txid { verified.txid },
                .aid { asset.id() },
                .total { o.amount },
                .filled { Funds_uint64::zero() },
                .limit { o.limit } });
        }

        MatchActions m(db, no, asset.info().id, poolLiquidity);

        history::MatchData d(asset.id(), poolLiquidity, m.pool);
        std::vector<AccountId> accounts;
        for (auto& s : m.buySwaps) {
            d.buy_swaps().push_back({ s.base, s.quote, s.oId });
            accounts.push_back(s.txid.accountId);
        }
        for (auto& s : m.sellSwaps) {
            d.sell_swaps().push_back({ s.base, s.quote, s.oId });
            accounts.push_back(s.txid.accountId);
        }
        matchDeltas.push_back(std::move(m));
        auto& ref { history.push_match(accounts, d, blockhash, asset.id()) };

        api.matches.push_back({ .txhash { ref.he.hash },
            .assetInfo { asset.info() },
            .liquidityBefore { poolLiquidity },
            .liquidityAfter { m.pool },
            .buySwaps {},
            .sellSwaps {} });
        auto b { api.matches.back() };
    }

    void process_liquidity_deposits(const AssetHandle& ah, const std::vector<LiquidityDepositsInternal>& deposits)
    {
        auto& pool { ah.get_pool(db) };
        for (auto& d : deposits) {
            auto v { verify(d) };
            auto shares { pool.deposit(d.basequote.base(), d.basequote.quote().E8()) };
            balanceChecker.add_balance(d.origin.id, ah.id().token_id(), shares);
            auto& ref { history.push_liquidity_deposit(v, shares, ah.id()) };
            api.liquidityDeposit.push_back(
                { .txhash { ref.he.hash },
                    .fee { v.liquidityAdd.fee() },
                    .baseDeposited { v.liquidityAdd.basequote.base() },
                    .quoteDeposited { v.liquidityAdd.basequote.quote() },
                    .sharesReceived { shares } });
        }
    }

    void process_liquidity_withdrawals(const AssetHandle& td, const std::vector<LiquidityWithdrawalInternal>& withdrawals)
    {
        auto& pool { td.get_pool(db) };
        for (auto& a : withdrawals) {
            auto v { verify(a) };
            auto w { pool.withdraw_liquity(a.poolShares) };
            if (!w)
                throw Error(EPOOLREDEEM);
            // credit withdrawn balance
            auto baseReceived { w->base };
            Wart quoteReceived { Wart::from_funds_throw(w->quote) };
            balanceChecker.add_balance(a.origin.id, td.id().token_id(), baseReceived);
            balanceChecker.add_balance(a.origin.id, TokenId::WART, quoteReceived);

            auto& ref { history.push_liquidity_withdrawal(v, baseReceived, quoteReceived, td.id()) };
            api.liquidityWithdrawal.push_back({
                .txhash { ref.he.hash },
                .fee { v.liquidityAdd.fee() },
                .sharesRedeemed { a.poolShares },
                .baseReceived { baseReceived },
                .quoteReceived { quoteReceived },
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
    PreparationGenerator(const BlockApplier::Preparer& preparer, const Block& b, const BlockHash& hash)
        : Preparation(preparer.db)
        , db(preparer.db)
        , hc(preparer.hc)
        , baseTxIds(preparer.baseTxIds)
        , newTxIds(preparer.newTxIds)
        , blockhash(hash)
        , body { b.body }
        , height(b.height)
        , reward(b.body.reward)
        , balanceChecker(AccountId(db.next_account_id()), StateId(db.next_state_id()), b.body, height)
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
        register_cancelations();
        register_token_sections();
        register_token_creations(); // token creation section

        /// Process block sections
        process_actions();
        process_balances();
    }
};

Preparation BlockApplier::Preparer::prepare(const Block& b, const Hash& h) const
{
    return PreparationGenerator(*this, b, h);
}

api::CompleteBlock BlockApplier::apply_block(const Block& block, const BlockHash& hash, BlockId blockId)
{
    auto prepared { preparer.prepare(block, hash) }; // call const function

    // ABOVE NO DB MODIFICATIONS
    //////////////////////////////
    // FOR EXCEPTION SAFETY //////
    // (ATOMICITY )         //////
    //////////////////////////////
    // BELOW NO "Error" TROWS

    auto update_wart_balance { [this](AccountToken at, Funds_uint64 bal) {
        if (at.token_id() == TokenId::WART) {
            WART_BalanceUpdates.insert_or_assign(at.account_id(), Wart::from_funds_throw(bal));
        }
    } };
    try {
        preparer.newTxIds.merge(std::move(prepared.txset));

        // create new pools
        // create new tokens

        // update old balances
        for (auto& [balId, accountToken, bal] : prepared.updateBalances) {
            db.set_balance(balId, bal);
            update_wart_balance(accountToken, bal);
        }

        for (auto& [address, accId] : prepared.insertAccounts) // new accounts
            db.insert_account(address, accId);

        for (auto& d : prepared.deleteOrders) // delete orders
            db.delete_order(d);
        for (auto& o : prepared.insertOrders) // new orders
            db.insert_order(o);
        for (auto& d : prepared.matchDeltas) {
            if (auto& o { d.orderBuyPartial })
                db.change_fillstate(*o, true);
            if (auto& o { d.orderSellPartial })
                db.change_fillstate(*o, false);
            for (auto& o : d.orderDeletes)
                db.delete_order(o);
            db.set_pool_liquidity(d.assetId, d.pool);
        }

        // insert token creations
        for (auto& tc : prepared.insertAssetCreations)
            db.insert_new_token(tc);

        // insert new balances
        for (auto& [at, bal] : prepared.insertBalances) {
            db.insert_token_balance(at, bal);
            update_wart_balance(at, bal);
        }

        // write undo data
        db.set_block_undo(blockId, prepared.rg.serialize());

        // write consensus data
        db.insert_consensus(block.height, blockId, db.next_history_id(), prepared.rg.next_state_id());

        prepared.historyEntries.write(db);
        return api::CompleteBlock(api::Block(block.header, block.height, 0, std::move(prepared.api)));
    } catch (Error e) {
        throw std::runtime_error(std::string("Unexpected exception: ") + __PRETTY_FUNCTION__ + ":" + e.strerror());
    }
}
}
