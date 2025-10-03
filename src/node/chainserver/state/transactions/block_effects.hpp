#pragma once
#include "block/body/rollback.hpp"
#include "chainserver/db/chain_db.hpp"
#include "chainserver/db/types.hpp"
#include "defi/token/account_token.hpp"
#include "general/funds.hpp"
#include <vector>

namespace block_apply {

struct AccountInsert : public chain_db::AccountData {
};

struct BalanceUpdate {
    AccountToken at;
    BalanceId id;
    Funds_uint64 original;
    Funds_uint64 updated;
};
struct BalanceInsert : public chain_db::BalanceData { };
struct OrderDelete : public chain_db::OrderData { };
struct OrderInsert : public chain_db::OrderData { };
struct PoolUpdate {
    PoolUpdate(const chain_db::PoolData& pd)
        : original(pd.asset_id(), pd)
        , updated(pd)
    {
    }
    bool nonzero() const
    {
        return updated.nonzero();
    }

    rollback::Poolstate original;
    defi::Pool_uint64 updated;
};

struct PoolInsert : public chain_db::PoolData {
    using chain_db::PoolData::PoolData;
};

struct OrderUpdate {
    chain_db::OrderFillstate newFillState;
    Funds_uint64 originalFilled;
    chain_db::OrderFillstate original_fill_state() const
    {
        return {
            .id { newFillState.id },
            .filled { originalFilled },
            .buy = newFillState.buy
        };
    }
};
struct AssetInsert : public chain_db::AssetData {
};

class TrackingDBApplier {
public:
    TrackingDBApplier(ChainDB& db, std::map<AccountId, Wart>& wartUpdates)
        : db(db)
        , wartUpdates(wartUpdates)
        , rg(db)
    {
    }
    void apply(const AccountInsert& a)
    {
        db.insert_guarded(a);
    }
    void apply(const BalanceUpdate& u)
    {
        if (u.at.token_id() == TokenId::WART)
            wartUpdates.insert_or_assign(u.at.account_id(), Wart::from_funds_throw(u.original));
        db.set_balance(u.id, u.updated);
        rg.register_original_balance({ u.id, u.original });
    }
    void apply(const BalanceInsert& b)
    {
        db.insert_token_balance(b);
    }
    void apply(OrderDelete od)
    {
        db.delete_order({ .id = od.id, .buy = od.buy });
        rg.register_original_order(std::move(od), db.next_history_id());
    }
    void apply(const chain_db::OrderData& o)
    {
        db.insert(o);
    }
    void apply(const OrderUpdate& o)
    {
        db.update_order_fillstate(o.newFillState);
        rg.register_original_fillstate(o.original_fill_state(), db.next_history_id());
    }
    void apply(const PoolInsert& p)
    {
        rg.register_newly_created_pool(p.asset_id());
        db.insert_pool(p);
    }
    void apply(const chain_db::AssetData& a)
    {
        db.insert_guarded(a);
    }
    void apply(const PoolUpdate& u)
    {
        rg.register_original_poolstate(u.original);
        db.update_pool({ u.original.id, u.updated });
    }
    void apply(const AssetInsert& a)
    {
    }
    auto rollback_data() &&
    {
        return std::move(rg);
    }

private:
    ChainDB& db;
    std::map<AccountId, Wart>& wartUpdates;
    rollback::Data rg;
};

template <typename... Ts>
struct BlockEffectsBase {
private:
    using variant_t = std::variant<Ts...>;
    std::vector<variant_t> data;

    auto apply(TrackingDBApplier db) const
    {
        for (auto& d : data)
            std::visit([&](auto& e) { db.apply(e); }, d);
        return std::move(db).rollback_data();
    }

public:
    template <typename T>
    requires(std::is_same_v<T, std::remove_cvref_t<Ts>> || ...)
    void insert(T&& t)
    {
        data.push_back(std::forward<T>(t));
    }

    auto apply2(ChainDB& db, std::map<AccountId, Wart>& wartUpdatess) const
    {
        return apply(TrackingDBApplier { db, wartUpdatess });
    }
};

using BlockEffects = BlockEffectsBase<AccountInsert, BalanceUpdate, BalanceInsert, OrderDelete, OrderInsert, OrderUpdate, PoolInsert, PoolUpdate, AssetInsert>;

// struct BlockEffects {
//     std::vector<AccountInsert> accountInserts;
//     std::vector<BalanceUpdate> balanceUpdates;
//     std::vector<BalanceInsert> balanceInserts;
//     std::vector<OrderDelete> orderDeletes;
//     std::vector<OrderInsert> orderInserts;
//     std::vector<OrderUpdate> orderUpdates;
//     std::vector<PoolInsert> poolInserts;
//     std::vector<PoolUpdate> poolUpdates;
//     std::vector<AssetInsert> assetInserts;
//
//     [[nodiscard]] auto apply(RollbackTrackingDB db)
//     {
//         // Checklist for different transaction types
//         // insertAccounts:       generate [ ], apply [x], rollback [ ]
//         // updateBalances:       generate [ ], apply [x], rollback [ ]
//         // insertBalances:       generate [ ], apply [x], rollback [ ]
//         // deleteOrders:         generate [ ], apply [x], rollback [ ]
//         // insertAssetCreations: generate [ ], apply [ ], rollback [ ]
//         // insertOrders:         generate [ ], apply [ ], rollback [ ]
//         // insertCancelOrder:    generate [ ], apply [ ], rollback [ ]
//         // matchDeltas:          generate [ ], apply [ ], rollback [ ]
//
//         auto apply { [&](const auto& v) { for (auto& e : v) db.apply(e); } };
//
//         // new accounts
//         apply(accountInserts);
//         apply(balanceUpdates);
//         apply(balanceInserts);
//         apply(orderDeletes);
//         apply(orderInserts);
//         apply(orderUpdates);
//         apply(poolInserts);
//         apply(poolUpdates);
//         apply(assetInserts);
//         return std::move(db).rollback_data();
//     }
// };
}
