#include "block_applier.hpp"
#include "api/types/all.hpp"
#include "block/body/rollback.hpp"
#include "block/chain/header_chain.hpp"
#include "block/chain/history/history.hpp"
#include "block_effects.hpp"
#include "chainserver/db/chain_db.hpp"
#include "chainserver/db/state_ids.hpp"
#include "chainserver/db/types.hpp"
#include "chainserver/state/block_apply/types.hpp"
#include "defi/token/account_token.hpp"
#include "defi/token/info.hpp"
#include "defi/uint64/lazy_matching.hpp"
#include "defi/uint64/pool.hpp"

using namespace block::body;

namespace chainserver {
namespace {

api::block::SignedInfoData make_signed_info(const auto& verified, HistoryId hid)
{
    return api::block::SignedInfoData {
        verified.hash,
        std::move(hid),
        verified.ref.origin.address,
        verified.ref.compactFee.uncompact(),
        verified.ref.pinNonce.id,
        verified.txid.pinHeight
    };
}

struct VerifiedOrderWithId : public block_apply::Order::Verified {
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
                o.ref.amount(),
                Funds_uint64::zero(),
                o.ref.limit()));
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
    OrderMergeLoader(loader_t loader, const UnsortedOrders<ASCENDING>& new_orders)
        : loader(std::move(loader))
        , newOrders(new_orders)
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
    OrderAggregator(loader_t loader, const UnsortedOrders<ASCENDING>& newOrders, const std::set<HistoryId>& ignoreOrderIds)
        : OrderAggregator(OrderMergeLoader<ASCENDING> { std::move(loader), newOrders }, ignoreOrderIds)
    {
    }
    OrderAggregator(OrderMergeLoader<ASCENDING> l, const std::set<HistoryId>& ignoreOrderIds)
        : l(std::move(l))
        , ignoreOrderIds(ignoreOrderIds)
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

        while (true) {
            std::optional<OrderData> o { l() };
            if (!o) {
                finished = true;
                return;
            }
            if (ignoreOrderIds.contains(o->id))
                continue;
            loaded.push_back(*o);
        }
    }

private:
    bool finished { false };
    std::vector<OrderData> loaded;
    OrderMergeLoader<ASCENDING> l;
    const std::set<HistoryId>& ignoreOrderIds;
};
using BuyOrderAggregator = OrderAggregator<false>;
using SellOrderAggregator = OrderAggregator<true>;

struct AggregatorMatch {
    SellOrderAggregator sellAscAggregator;
    BuyOrderAggregator buyDescAggregator;
    defi::MatchResult_uint64 m;
    AggregatorMatch(const ChainDB& db, const UnsortedOrderbook& unsortedOrderbook, AssetId aid, const defi::PoolLiquidity_uint64& p, const std::set<HistoryId>& ignoreOrderIds)
        : sellAscAggregator { db.base_order_loader_ascending(aid), unsortedOrderbook.sells, ignoreOrderIds }
        , buyDescAggregator { db.quote_order_loader_descending(aid), unsortedOrderbook.buys, ignoreOrderIds }
        , m { defi::match_lazy(sellAscAggregator, buyDescAggregator, p) }
    {
    }
};

struct MatchProcessor {

    struct Swaps {
        std::vector<SwapInternal> buySwaps;
        std::vector<SwapInternal> sellSwaps;
    };

    Swaps match(defi::PoolLiquidity_uint64& pool, block_apply::BlockEffects* blockEffects = nullptr) const
    {
        Swaps res;
        AggregatorMatch am { db, unsortedOrderbook, assetId, pool, ignoreOrderIds };
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
            for (const auto& o : am.sellAscAggregator.loaded_orders()) {
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

                // order swapped b -> q
                if (orderFilled >= o.order.amount) {
                    assert(orderFilled == o.order.amount);
                    if (blockEffects) {
                        blockEffects->orderDeletes.push_back({
                            .id = o.id,
                            .buy = false,
                            .txid = o.txid,
                            .aid = assetId,
                            .total = o.order.amount,
                            .filled = o.filled,
                            .limit = o.order.limit,
                        });
                    }
                } else {
                    assert(remaining == 0);
                    if (blockEffects)
                        blockEffects->orderUpdates.push_back({ .newFillState { .id { o.id }, .filled { orderFilled }, .buy = false }, .originalFilled { o.filled } });
                }
                res.sellSwaps.push_back(SwapInternal { .oId { o.id }, .txid { o.txid }, .base { b }, .quote { Wart::from_funds_throw(*q) } });
            }
            assert(remaining == 0);
            returned.quote.subtract_assert(quoteDistributed);
        }
        if (m.filled.quote != 0) { // buyer match
            Nonzero_uint64 filledQuote { m.filled.quote.value() };
            Funds_uint64 baseDistributed { 0 };
            auto remaining { m.filled.quote };
            for (const auto& o : am.buyDescAggregator.loaded_orders()) {
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
                    if (blockEffects) {
                        blockEffects->orderDeletes.push_back({
                            .id = o.id,
                            .buy = true,
                            .txid = o.txid,
                            .aid = assetId,
                            .total = o.order.amount,
                            .filled = o.filled,
                            .limit = o.order.limit,
                        });
                    }
                } else {
                    assert(remaining == 0);
                    if (blockEffects)
                        blockEffects->orderUpdates.push_back({ .newFillState { .id { o.id }, .filled { orderFilled }, .buy = true }, .originalFilled { o.filled } });
                }
                res.buySwaps.push_back(SwapInternal { .oId { o.id }, .txid { o.txid }, .base { *b }, .quote { Wart::from_funds_throw(q) } });
            }
            assert(remaining == 0);
            returned.base.subtract_assert(baseDistributed);
        }
        return res;
    }
    MatchProcessor(AssetId assetId, const ChainDB& db, const std::set<HistoryId>& ignoreOrderIds, const UnsortedOrderbook& unsortedOrderbook)
        : assetId(assetId)
        , db(db)
        , ignoreOrderIds(ignoreOrderIds)
        , unsortedOrderbook(unsortedOrderbook)
    {
    }

private:
    AssetId assetId;
    const ChainDB& db;
    const std::set<HistoryId>& ignoreOrderIds;
    const UnsortedOrderbook& unsortedOrderbook;
};

struct NewOrdersInternal : UnsortedOrderbook {
    bool empty() const { return buys.empty() && sells.empty(); }
    void push_back(VerifiedOrderWithId o)
    {
        if (o.ref.buy())
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
    std::vector<block_apply::TokenTransfer::Internal> assetTransfers;
    std::vector<block_apply::TokenTransfer::Internal> sharesTransfers;
    std::vector<block_apply::Order::Internal> orders;
    std::vector<block_apply::LiquidityDeposit::Internal> liquidityAdds;
    std::vector<block_apply::LiquidityWithdrawal::Internal> liquidityRemoves;
    TokenSectionInternal(AssetId id)
        : id(id)
    {
    }
};

class AccountAndAssetIncrementer {
    StateId32 next;

public:
    AccountAndAssetIncrementer(StateId32 next)
        : next(next)
    {
    }
    AssetId next_asset_id_inc() { return AssetId((next++).value()); }
    AccountId next_account_id_inc() { return AccountId((next++).value()); }
    AccountId next_account_id_state() const { return AccountId(next.value()); }
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
    struct AccountData : public block_apply::ValidAccount {
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
        AccountData(AddressView address, ValidAccountId accountId)
            : ValidAccount(address, accountId)
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
        Accounts(AccountAndAssetIncrementer& inc, const Body& b)
            : beginNew(inc.next_account_id_state())
            , beginInvalid(beginNew + b.newAddresses.size())
        {
            size_t n { b.newAddresses.size() };
            newAccounts.reserve(n);
            for (size_t i = 0; i < n; ++i)
                newAccounts.push_back({ b.newAddresses[i], (inc.next_account_id_inc()).validate_throw(beginInvalid) });
        }
        [[nodiscard]] AccountData& operator[](AccountId id)
        {
            auto vid { id.validate_throw(beginInvalid) };
            if (id < beginNew) {
                return oldAccounts.try_emplace(vid, vid).first->second;
            } else {
                assert(id < beginInvalid);
                return newAccounts[vid.value() - beginNew.value()];
            }
        }
        auto& new_accounts() const { return newAccounts; }
        auto& old_accounts() { return oldAccounts; }

    private:
        AccountId beginNew;
        AccountId beginInvalid;
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
    static RewardInternal register_reward(const Body& b, Accounts& accounts, NonzeroHeight h)
    {
        auto r { b.reward };
        auto& a { accounts[r.to_id()] };
        auto am { r.wart() };
        a.add(TokenId::WART, am);
        return {
            .toAccountId { a.id },
            .wart { am },
            .height { h },
            .toAddress { a.address }
        };
    }
    BalanceChecker(AccountAndAssetIncrementer& inc, const Body& b, NonzeroHeight height)
        : height(height)
        , pinFloor(height.pin_floor())
        , b(b)
        , accounts(inc, b)
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
        assert(from.id == v.origin_account_id());
        charge_fee(from, compactFee);
        return {
            SignerData { from.id, from.address, v.pin_nonce(), compactFee, v.signature() },
            from
        };
    }
    [[nodiscard]] auto& __register_transfer(TokenId tokenId, AccountId toId, Funds_uint64 amount, const ProcessedSigner& s) // OK
    {
        auto& to { accounts[toId] };
        if (height.value() > 719118 && amount.is_zero())
            throw Error(EZEROAMOUNT);
        if (s.origin.id == to.id)
            throw Error(ESELFSEND);

        to.add(tokenId, amount);
        s.account.subtract(tokenId, amount);

        return to;
    }
    void register_wart_transfer(const WartTransfer& tv)
    {
        auto s(process_signer(tv));
        auto toAcc { __register_transfer(TokenId::WART, tv.to_id(), tv.wart(), s) };
        wartTransfers.push_back({ std::move(s), { std::move(toAcc), tv.wart() } });
    }

    void register_cancelation(const Cancelation& c)
    {
        auto s(process_signer(c));
        cancelations.push_back({ std::move(s), { c.canceled_txid() } });
    }

    block_apply::LiquidityDeposit::Internal register_liquidity_deposit(const LiquidityDeposit& l, AssetId aid)
    {
        auto s { process_signer(l) };
        s.account.subtract(aid.token_id(), l.base_amount());
        s.account.subtract(TokenId::WART, l.quote_wart());
        return { std::move(s), { defi::BaseQuote { l.base_amount(), l.quote_wart() }, aid } };
    }

    void add_balance(AccountId aid, TokenId tid, Funds_uint64 amount)
    {
        accounts[aid].add(tid, amount);
    }

    block_apply::LiquidityWithdrawal::Internal register_liquidity_withdraw(const LiquidityWithdraw& l, AssetId aid)
    {
        auto s { process_signer(l) };
        s.account.subtract(aid.token_id(true), l.amount());
        return { std::move(s), { l.amount(), aid } };
    }
    block_apply::Order::Internal register_new_order(const Order& o, AssetId aid)
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
            { o.limit(), amount, buy, aid }
        };
    }
    void register_token_section(const block::body::elements::tokens::TokenSection& t)
    {
        auto aid { t.asset_id() };
        TokenSectionInternal ts(t.asset_id());
        for (auto& tr : t.asset_transfers()) {
            auto s(process_signer(tr));
            auto valid_to_id { __register_transfer(aid.token_id(), tr.to_id(), tr.amount(), s) };
            ts.assetTransfers.push_back({ s, { valid_to_id, tr.amount(), aid } });
        }
        for (auto& tr : t.share_transfers()) {
            auto s(process_signer(tr));
            auto valid_to_id { __register_transfer(aid.token_id(true), tr.to_id(), tr.shares(), s) };
            ts.sharesTransfers.push_back({ s, { valid_to_id, tr.shares(), aid } });
        }
        for (auto& o : t.orders())
            ts.orders.push_back(register_new_order(o, aid));
        for (auto& a : t.liquidity_deposits())
            ts.liquidityAdds.push_back(register_liquidity_deposit(a, t.asset_id()));
        for (auto& r : t.liquidity_withdrawals())
            ts.liquidityRemoves.push_back(register_liquidity_withdraw(r, t.asset_id()));
        tokenSections.push_back(std::move(ts));
    }

    void register_asset_creation(const AssetCreation& tc, Height)
    {
        auto s { process_signer(tc) };
        assetCreations.push_back(block_apply::AssetCreation::Internal {
            s,
            // index,
            {
                tc.asset_name(),
                tc.supply(),
            } });
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
    auto& asset_creations() const
    {
        return assetCreations;
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
    const Body& b;
    Accounts accounts;
    RewardInternal reward;

    std::vector<block_apply::WartTransfer::Internal> wartTransfers;
    std::vector<block_apply::Cancelation::Internal> cancelations;
    std::vector<TokenSectionInternal> tokenSections;
    std::vector<block_apply::AssetCreation::Internal> assetCreations;
    Wart totalfee { Wart::zero() };
};

struct InsertHistoryEntry {
    InsertHistoryEntry(const RewardInternal& p, HistoryId historyId)
        : he(p)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(const block_apply::TokenTransfer::Verified& t, NonWartTokenId tokenId, HistoryId historyId)
        : he(t, tokenId)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(const block_apply::Order::Verified& t, HistoryId historyId)
        : he(t)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(const block_apply::Cancelation::Verified& t, HistoryId historyId)
        : he(t)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(const block_apply::WartTransfer::Verified& t, HistoryId historyId)
        : he(t)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(const block_apply::AssetCreation::Verified& t, AssetId assetId, HistoryId historyId)
        : he(t, assetId)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(const block_apply::LiquidityDeposit::Verified& t, Funds_uint64 receivedShares, HistoryId historyId)
        : he(t, receivedShares)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(const block_apply::LiquidityWithdrawal::Verified& t, Funds_uint64 receivedBase, Wart receivedQuote, HistoryId historyId)
        : he(t, receivedBase, receivedQuote)
        , historyId(historyId)
    {
    }
    InsertHistoryEntry(history::MatchData md, TxHash h, HistoryId historyId)
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
            auto inserted { db.insertHistory(p.he.hash, p.he.data.to_bytes()) };
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
    [[nodiscard]] auto& push_wart_transfer(const block_apply::WartTransfer::Verified& r)
    {
        return for_accounts(r.ref.origin.id, r.ref.to_id()).insert_history(r);
    }
    [[nodiscard]] auto& push_order(const block_apply::Order::Verified& r)
    {
        return for_account(r.ref.origin.id).insert_history(r);
    }
    [[nodiscard]] auto& push_cancelation(const block_apply::Cancelation::Verified& r)
    {
        return for_account(r.ref.origin.id).insert_history(r);
    }
    [[nodiscard]] auto& push_token_transfer(const block_apply::TokenTransfer::Verified& r, NonWartTokenId tokenId)
    {
        return for_accounts(r.ref.origin.id, r.ref.to_id()).insert_history(r, tokenId);
    }
    [[nodiscard]] auto& push_asset_creation(const block_apply::AssetCreation::Verified& t, AssetId assetId)
    {
        return for_account(t.ref.origin.id).insert_history(t, assetId);
    }
    [[nodiscard]] auto& push_match(const std::vector<AccountId>& accounts, const history::MatchData& t, const BlockHash& blockHash, AssetId assetId)
    {
        return for_accounts(accounts).insert_history(t, TxHash(hash_args_SHA256(blockHash, assetId)));
    }
    [[nodiscard]] auto& push_liquidity_deposit(const block_apply::LiquidityDeposit::Verified& v, Funds_uint64 sharesReceived)
    {
        return for_account(v.ref.origin.id).insert_history(v, sharesReceived);
    }
    [[nodiscard]] auto& push_liquidity_withdrawal(const block_apply::LiquidityWithdrawal::Verified& v, Funds_uint64 baseReceived, Wart quoteReceived)
    {
        return for_account(v.ref.origin.id).insert_history(v, baseReceived, quoteReceived);
    }
};

}

class Preparation {

public:
    HistoryEntries historyEntries;
    std::set<TransactionId> txset;
    block_apply::BlockEffects blockEffects;
    api::block::Actions api;

    void write_wart_updates(std::map<AccountId, Wart>& wartUpdates) const
    {
        for (auto& u : blockEffects.updateBalances) {
            if (u.at.token_id() == TokenId::WART)
                wartUpdates.insert_or_assign(u.at.account_id(), Wart::from_funds_throw(u.original));
        }
    }

private:
    friend class PreparationGenerator;
    Preparation()
    {
    }
};

class PreparationGenerator : public Preparation {

private:
    // constants
    const ChainDB& db;
    const Headerchain& hc;
    AccountAndAssetIncrementer idIncrementer;
    decltype(BlockApplier::Preparer::baseTxIds)& baseTxIds;
    const decltype(BlockApplier::Preparer::newTxIds)& newTxIds;
    const BlockHash& blockhash;
    const Body& body;
    const NonzeroHeight height;

    // variables needed for block verification
    const Reward reward;
    BalanceChecker balanceChecker;
    HistoryEntriesGenerator history;
    TransactionVerifier txVerifier;
    std::set<TransactionId> canceledTxids;
    std::set<HistoryId> ignoreOrderIds;

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
        for (auto t : body.wart_transfers())
            balanceChecker.register_wart_transfer(t);
    }
    void register_cancelations()
    {
        for (auto& c : body.cancelations())
            balanceChecker.register_cancelation(c);
    }
    void register_token_sections()
    {
        for (auto t : body.tokens())
            balanceChecker.register_token_section(t);
    }

    void register_asset_creations()
    {
        for (size_t i { 0 }; i < body.asset_creations().size(); ++i)
            balanceChecker.register_asset_creation(body.asset_creations()[i], height);

        auto& assetCreations { balanceChecker.asset_creations() };
        for (size_t i { 0 }; i < assetCreations.size(); ++i) {
            auto& ac { assetCreations[i] };
            auto assetId { idIncrementer.next_asset_id_inc() };
            const auto verified { ac.verify(txVerifier) };
            blockEffects.insertAssetCreations.push_back(chain_db::AssetData {
                .id { assetId },
                .height { height },
                .ownerAccountId { ac.origin.id },
                .supply { ac.supply() },
                .groupId { assetId.token_id() },
                .parentId { TokenId { 0 } },
                .name { ac.asset_name() },
                .hash { AssetHash(TxHash(verified.hash)) },
                .data {} });

            auto& ref { history.push_asset_creation(verified, assetId) };
            api.assetCreations.push_back({ make_signed_info(verified, ref.historyId), { .name { ac.asset_name() }, .supply { ac.supply() }, .assetId { assetId } } });
        }
    }

    auto adjust_new_balance(const AccountToken& at, const auto& tokenFlow)
    {
        if (tokenFlow.out() > Funds_uint64::zero()) // We do not allow resend of newly inserted balance
            throw Error(EBALANCE); // insufficient balance
        Funds_uint64 balance { tokenFlow.in() };
        blockEffects.insertBalances.push_back({ at, balance });
    }
    auto adjust_existing_balance(const AccountToken& at, const auto& tokenFlow, IdBalance ib)
    {
        // check that balances are correct
        auto totalIn { Funds_uint64::sum_throw(tokenFlow.in(), ib.balance) };
        Funds_uint64 newbalance { Funds_uint64::diff_throw(totalIn, tokenFlow.out()) };
        blockEffects.updateBalances.push_back(
            { .at { at },
                .id { ib.id },
                .original { ib.balance },
                .updated { newbalance } });
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
                if (auto [balanceId, balance] { db.get_token_balance_recursive(accountId, tokenId) }; balanceId)
                    adjust_existing_balance(at, tokenFlow, { *balanceId, balance });
                else
                    adjust_new_balance(at, tokenFlow);
            }
        }

        // process new accounts
        for (auto& a : balanceChecker.get_new_accounts()) {
            bool referred { false };
            for (auto& [tokenId, tokenFlow] : a.token_flow()) {
                if (!tokenFlow.in().is_zero())
                    referred = true;
                adjust_new_balance({ a.id, tokenId }, tokenFlow);
            }
            if (!referred)
                throw Error(EIDPOLICY); // id was not referred
            blockEffects.insertAccounts.push_back({ a.address, a.id });
        }
    }

    void process_actions()
    {
        process_reward();
        process_cancelations(); // cancelations must be processed first
        process_wart_transfers();
        process_token_sections();
    }

    void process_reward()
    {
        const auto& balanceChecker { this->balanceChecker }; // shadow balanceChecker

        auto& r { balanceChecker.get_reward() };
        if (r.wart > Wart::sum_throw(height.reward(), balanceChecker.getTotalFee()))
            throw Error(EBALANCE);
        assert(!r.toAddress.is_null());
        auto& ref { history.push_reward(r) };
        api.reward = api::block::Reward {
            ref.he.hash,
            ref.historyId,
            {
                .toAddress { r.toAddress },
                .wart { r.wart },
            }
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
            block_apply::PoolUpdate pool;
        };
        AssetHandle(AssetBasic t)
            : _info(std::move(t))
        {
        }
        auto& info() const { return _info; }
        auto& hash() const { return info().hash; }
        auto& precision() const { return info().precision; }
        auto id() const { return _info.id; }
        auto& name() const { return _info.name; }
        [[nodiscard]] auto& get_pool(const ChainDB& db)
        {
            if (!o) {
                if (auto p { db.select_pool(info().id) })
                    o.emplace(LoadedPool { false, *p });
                else
                    o.emplace(LoadedPool { true, PoolData::zero(id()) });
            }
            return o->pool.updated;
        }
        auto& loaded_pool() const { return o; }

    private:
        AssetBasic _info;
        mutable std::optional<LoadedPool> o;
    };
    void process_token_sections()
    {
        auto ts { balanceChecker.get_token_sections() };
        for (auto& ts : balanceChecker.get_token_sections()) {
            auto ihn { db_asset(ts.asset_id()) };
            AssetHandle ah(ihn);
            process_token_transfers(ah, ts.sharesTransfers);
            match_new_orders(ah, ts.orders);
            process_liquidity_deposits(ah, ts.liquidityAdds);
            process_liquidity_withdrawals(ah, ts.liquidityRemoves);
            if (auto& o { ah.loaded_pool() }) {
                auto& p { *o };
                if (p.create && p.pool.nonzero())
                    blockEffects.poolInsertions.push_back({ ah.id(), p.pool.updated });
                else
                    blockEffects.poolUpdates.push_back(o->pool);
            }
        }
    }
    // template <typename... T>
    // [[nodiscard]] auto verify(auto& tx, T&&... t)
    // {
    //     return tx.verify(txVerifier, std::forward<T>(t)...);
    // }
    // generate history for transfers and check signatures
    // and check for unique transaction ids
    void process_wart_transfers()
    {
        const auto& balanceChecker { this->balanceChecker }; // shadow balanceChecker
        for (auto& tr : balanceChecker.get_wart_transfers()) {
            auto verified { tr.verify(txVerifier) };

            auto& hid { history.push_wart_transfer(verified).historyId };
            api.wartTransfers.push_back(api::block::WartTransfer {
                make_signed_info(verified, hid),
                {
                    .toAddress { tr.to_address() },
                    .amount { tr.wart() },
                } });
        }
    }

    void process_cancelations()
    {
        const auto& balanceChecker { this->balanceChecker }; // const lock balanceChecker
        for (auto& c : balanceChecker.get_cancelations()) {
            auto verified { c.verify(txVerifier) };
            auto hid { history.push_cancelation(verified).historyId };
            api.cancelations.push_back({ make_signed_info(verified, hid), {} });
            canceledTxids.insert(verified.ref.cancel_txid());
            auto o { db.select_order(verified.ref.cancel_txid()) };
            if (o) { // transaction is removed from the database
                ignoreOrderIds.insert(o->id);
                blockEffects.orderDeletes.push_back(*o);
            }
        }
    }

    void process_token_transfers(AssetHandle& token, const std::vector<block_apply::TokenTransfer::Internal>& transfers)
    {
        for (auto& tr : transfers) {
            auto verified { tr.verify(txVerifier, token.info().hash) };
            auto& ref { history.push_token_transfer(verified, token.id().token_id()) };
            api.tokenTransfers.push_back(api::block::TokenTransfer {
                make_signed_info(verified, ref.historyId),
                {
                    .assetInfo { token.info() },
                    .toAddress { tr.to_address() },
                    .amount { tr.amount() },
                } });
        }
    }
    [[nodiscard]] NewOrdersInternal generate_new_orders(const AssetHandle& asset, const std::vector<block_apply::Order::Internal>& orders, const std::set<TransactionId>& canceled)
    {
        NewOrdersInternal res;
        for (auto& o : orders) {
            auto verified { o.verify(txVerifier, asset.hash()) };
            if (canceled.contains(verified.txid))
                continue;
            auto& ref { history.push_order(verified) };
            api.newOrders.push_back(api::block::NewOrder {
                make_signed_info(verified, ref.historyId),
                {
                    .assetInfo { asset.info() },
                    .amount { o.amount() },
                    .limit { o.limit() },
                    .buy = o.buy(),
                } });
            res.push_back({ verified, ref.historyId });
            blockEffects.OrderInsertions.push_back(chain_db::OrderData {
                .id { ref.historyId },
                .buy = o.buy(),
                .txid { verified.txid },
                .aid { asset.id() },
                .total { o.amount() },
                .filled { Funds_uint64::zero() },
                .limit { o.limit() } });
        }
        return res;
    }
    void match_new_orders(AssetHandle& asset, const std::vector<block_apply::Order::Internal>& orders)
    {
        if (orders.size() == 0)
            return;
        auto newOrders { generate_new_orders(asset, orders, canceledTxids) };

        auto& pool { asset.get_pool(db) };

        MatchProcessor m(asset.id(), db, ignoreOrderIds, newOrders);

        const defi::PoolLiquidity_uint64 poolBeforeMatch { pool };
        auto swaps { m.match(pool, &blockEffects) };

        // generate history entry for swap
        history::MatchData h(asset.id(), poolBeforeMatch, pool);
        std::vector<AccountId> accounts;
        for (auto& s : swaps.buySwaps) {
            h.buy_swaps().push_back({ s.base, s.quote, s.oId });
            accounts.push_back(s.txid.accountId);
        }
        for (auto& s : swaps.sellSwaps) {
            h.sell_swaps().push_back({ s.base, s.quote, s.oId });
            accounts.push_back(s.txid.accountId);
        }
        auto& ref { history.push_match(accounts, h, blockhash, asset.id()) };

        api.matches.push_back(
            api::block::Match {
                ref.he.hash,
                ref.historyId,
                api::block::MatchData {
                    asset.info(),
                    h.pool_before(),
                    h.pool_after(),
                    h.buy_swaps(),
                    h.sell_swaps(),
                },
            });
        auto b { api.matches.back() };
    }

    void process_liquidity_deposits(AssetHandle& ah, const std::vector<block_apply::LiquidityDeposit::Internal>& deposits)
    {
        auto& pool { ah.get_pool(db) };
        for (auto& d : deposits) {
            auto verified { d.verify(txVerifier, ah.hash()) };
            auto shares { pool.deposit(d.base(), d.quote().E8()) };
            balanceChecker.add_balance(d.origin.id, ah.id().token_id(), shares);
            auto& ref { history.push_liquidity_deposit(verified, shares) };
            api.liquidityDeposit.push_back(
                { make_signed_info(verified, ref.historyId),
                    api::block::LiquidityDepositData {
                        .assetInfo { ah.info() },
                        .baseDeposited { verified.ref.base() },
                        .quoteDeposited { verified.ref.quote() },
                        .sharesReceived = shares } });
        }
    }

    void process_liquidity_withdrawals(AssetHandle& ah, const std::vector<block_apply::LiquidityWithdrawal::Internal>& withdrawals)
    {
        auto& pool { ah.get_pool(db) };
        for (auto& a : withdrawals) {
            auto verified { a.verify(txVerifier, ah.hash()) };
            auto w { pool.withdraw_liquity(a.amount()) };
            if (!w)
                throw Error(EPOOLREDEEM);
            // credit withdrawn balance
            auto baseReceived { w->base };
            Wart quoteReceived { Wart::from_funds_throw(w->quote) };
            balanceChecker.add_balance(a.origin.id, ah.id().token_id(), baseReceived);
            balanceChecker.add_balance(a.origin.id, TokenId::WART, quoteReceived);

            auto& ref { history.push_liquidity_withdrawal(verified, baseReceived, quoteReceived) };
            api.liquidityWithdrawal.push_back(
                { make_signed_info(verified, ref.historyId),
                    {
                        .assetInfo { ah.info() },
                        .sharesRedeemed { a.amount() },
                        .baseReceived { baseReceived },
                        .quoteReceived { quoteReceived },
                    } });
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
        : Preparation()
        , db(preparer.db)
        , hc(preparer.hc)
        , idIncrementer(db.next_id32())
        , baseTxIds(preparer.baseTxIds)
        , newTxIds(preparer.newTxIds)
        , blockhash(hash)
        , body { b.body }
        , height(b.height)
        , reward(b.body.reward)
        , balanceChecker(idIncrementer, b.body, height)
        , history(historyEntries, db.next_history_id())
        , txVerifier(TransactionVerifier { hc, height,
              std::function<bool(TransactionId)>(
                  [this](TransactionId tid) -> bool {
                      return verify_txid(tid);
                  }) })
    {

        /// Read block sections
        verify_new_address_policy(); // new address section
        register_cancelations(); // cancelations must be registered first after other tranasactions because we build the list of cancelations there
        register_wart_transfers(); // WART transfer section
        register_token_sections();
        register_asset_creations(); // token creation section

        /// Process block sections
        process_actions();
        process_balances();

        // merge canceled transaction ids
        txset.merge(std::move(canceledTxids));
    }
};

Preparation BlockApplier::Preparer::prepare(const Block& b, const BlockHash& h) const
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

    try {
        // write WART balance updates in the cumulative map 'wartUpdates' of all blocks applied so far
        prepared.write_wart_updates(wartUpdates);

        // merge transaction ids of this block into  newTxIds
        preparer.newTxIds.merge(std::move(prepared.txset));

        // write block effects to database
        auto rollback { prepared.blockEffects.apply(db) };

        // write rollback data
        db.set_block_undo(blockId, rollback.serialize());

        // write consensus data
        db.insert_consensus(block.height, blockId, db.next_history_id(), rollback.next_state_id32());

        // write history entries
        prepared.historyEntries.write(db);

        return api::CompleteBlock(api::Block(block.header, block.height, 0, std::move(prepared.api)));
    } catch (Error e) {
        throw std::runtime_error(std::string("Unexpected exception: ") + __PRETTY_FUNCTION__ + ":" + e.strerror());
    }
}
}
